#include <cstdio>
#include <list>
#include <mutex>
#include <vector>
#include <gpio.h>
#include <esp_timer.h>
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"

#include "ir_rx.h"
#include <app_utils.h>
#include <morse.h>
#include <main.h>

#include "../include/OneLedBlinker.h"
#include "CoapClient.h"
#include "request.h"
#include "connect.h"
#include <device_message.h>
#include <ExpBackoff.h>
#include <MonitorMemory.h>
#include <config.h>

extern user_pass_t connection_data;
extern struct production_struct device_keys;


static std::list<command *> post_queue;
static std::list<device_message *> cmd_queue;
static std::mutex hold_posts;
static std::mutex hold_commands;
static bool inProximity = false;

static OneLedBlinker backLight_switch(
        []() {
            gpio_set_direction(PIN_BKL, GPIO_MODE_OUTPUT);
        },
        [](INDICATOR_PIN led, SWITCH_STATE state, void *report) {
            gpio_set_level(PIN_BKL, state);
            if (led == REL_PIN) {
                command *c;
                if (report) {
                    c = (command *) report;
                    c->value = state;
                } else
                    c = new command{BCKLT_SENSOR, (uint8_t) state};
                append_to_post_queue(c);
            }
        });

static void led_switcher_control(INDICATOR_PIN led, SWITCH_STATE state) {
    if (led == ON_PIN)
        gpio_set_level(PIN_GREEN, state);
    else if (led == OFF_PIN)
        gpio_set_level(PIN_YELLOW, state);
}

static Blinker light_switch(
        []() {
            gpio_set_direction(PIN_GREEN, GPIO_MODE_OUTPUT);
            gpio_set_direction(PIN_YELLOW, GPIO_MODE_OUTPUT);
            gpio_set_direction(PIN_RELAY, GPIO_MODE_OUTPUT);
            //todo: remove
            gpio_set_level(PIN_RELAY, STATE_OFF);
        },
        [](INDICATOR_PIN led, SWITCH_STATE state, void *report) {
            if (led == REL_PIN) {
                gpio_set_level(PIN_RELAY, state);
                if (state == STATE_ON && backLight_switch.get_state() == STATE_ON)
                    backLight_switch.switch_on(STATE_OFF);
                command *c;
                if (report) {
                    c = (command *) report;
                    c->value = (uint8_t) state;
                } else
                    c = new command(RELAY_SENSOR, (uint8_t) state);
                append_to_post_queue(c);
            } else
                led_switcher_control(led, state);
        }
);

static Morse player(
        [](SWITCH_STATE state) {
            led_switcher_control(light_switch.get_on_led(), state);
        },
        [] {
            led_switcher_control(light_switch.get_on_led(), STATE_ON);
        }
);

bool ExecuteDelayedTask(void (*func)(), uint8_t seconds, uint8_t stkBlocks, const char *name, uint8_t priority) //
{
    struct delayed_fun_t {
        void (*f)();

        uint16_t s;
    };
    BaseType_t res = xTaskCreate([](void *par) {
        auto *ds = (delayed_fun_t *) par;
        auto func = ds->f;
        int secs = ds->s;
        delete ds;
        if (secs > 0)
            vTaskDelay(secs * 1000 / portTICK_RATE_MS);
        func();
        vTaskDelete(nullptr);
    }, name, 512 * (stkBlocks > 0 ? stkBlocks : 1), new delayed_fun_t{.f=func, .s = seconds}, priority, nullptr);
    if (res != pdPASS)
        printf("Execute(%s) failed\n", name);
    return res == pdPASS;
}

bool ExecuteDelayedTask(void (*func)(), uint8_t seconds, uint8_t stkBlocks, const char *name) {
    return ExecuteDelayedTask(func, seconds, stkBlocks, name, (uint8_t) 10);
}

void append_to_post_queue(command *post) {
    hold_posts.lock();
    auto it = post_queue.cbegin();
    while (it != post_queue.cend()) {
        if ((*it)->sensor == post->sensor) {
            command *c = *it;
            it = post_queue.erase(it);
            delete c;
            break;
        } else it++;
    }
    post_queue.push_back(post);
    hold_posts.unlock();
}

unsigned char cid[] = "clientId";


// session must be initialized during authentication ... and used during calls
bool network_activity_detection = false;

[[noreturn]] void outputs_loop() {
    MonitorMemory mon("outputs_loop");
    device_message *am;
    command *c;
    size_t length = 0;
    std::vector<device_message *> *posting_items = new std::vector<device_message *>();

    while (true) {
        WAIT_FOR_NETWORK();
        if (get_session() == nullptr || !is_session_established(get_session()) || post_queue.empty()) {
            vTaskDelay(1000 / portTICK_RATE_MS);
            continue;
        }
        //queue is not empty, memory is low
        if (mon.heap() < 10000) {
            mon.update();
            vTaskDelay(100 / portTICK_RATE_MS);
            continue;
        }
        WAIT_FOR_NETWORK(); // double check

        if (hold_posts.try_lock()) {  // release lock as fast as possible
            unsigned char *result = nullptr;
            std::_List_const_iterator<command *> it;
            it = post_queue.cbegin();
            while (it != post_queue.cend()) { // clear list
                c = *it;
                am = new device_message();
                am->cmd = c->data;

                am->from = new app_address();
                STR_TO_BIN_T(am->from->topic, "device", strlen("device"));
                STR_TO_BIN_T(am->from->key, device_keys.identity, strlen(device_keys.identity));

                //am->to must be filled in response to incoming message, otherwise null
                if (c->to.topic.length > 0) {
                    am->to = new app_address();
                    STR_TO_BIN_T(am->to->topic, c->to.topic.s, c->to.topic.length);
                    STR_TO_BIN_T(am->to->key, c->to.key.s, c->to.key.length);
                }
                it = post_queue.erase(it);
                delete c;
                posting_items->push_back(am);
            }
            hold_posts.unlock();
            device_message::to_binary_t(posting_items, result, length);
            for (auto &item : *posting_items)
                delete item;
            posting_items->clear();
            assert(result != nullptr);
            Request *post = Request::new_post()
                    ->setSession(get_session())
                    ->set_payload(result, length)
                    ->setUrl(get_report_resource_url())
                    ->OnError([](const char *errText, uint errNo) {
                        printf("post error: %s\n", errText);
                    })
                    ->Timeout(1);
            free(result);
            post->SendAndWait(); // wait for 1 seconds, posts will pile up
            Request::release(post);
            network_activity_detection = true;
        }
    }
}


[[noreturn]] void inputs_loop() {
    //MonitorMemory mon("execute_cmds");
    while (true) {
        if (!cmd_queue.empty())
            if (hold_commands.try_lock()) {
                while (!cmd_queue.empty()) {
                    device_message *dm = cmd_queue.front();
                    //dm->from
                    cmd_queue.pop_front();
                    command *c = new command(dm->cmd);
                    STR_TO_BIN_T(c->to.topic, dm->from->topic.s, dm->from->topic.length);
                    STR_TO_BIN_T(c->to.key, dm->from->key.s, dm->from->key.length);
                    delete dm;
                    //mon.update();
                    switch (c->sensor) {
                        case RELAY_SENSOR :// on|off switch
                            light_switch.switch_on(c->value > 0, c);
                            break;
                        case BCKLT_SENSOR:// backlighti
                            backLight_switch.switch_on(c->value > 0 ? STATE_ON : STATE_OFF, c);
                            break;
                        case CTRL_SENSOR://  report, reset_wifi_data
                            if (c->value == 0) {
                                append_to_post_queue(new command(BCKLT_SENSOR, backLight_switch.get_state()));
                                append_to_post_queue(new command(RELAY_SENSOR, light_switch.get_state()));
                            } else if (c->value == 1) { //reset_wifi_data
                                connection_data.ssid[0] = 0;
                                connection_data.passwd[0] = 0;
                                //due to stack usage
                                xTaskCreate([](void *) {
                                    write_connection_data_to_flash(&connection_data);
                                    wifi_disconnect();
                                    vTaskDelete(nullptr);
                                }, "reset config", 2048, nullptr, 5, nullptr);
                            }
                            break;
                        default:
                            break;
                    }
                }
                hold_commands.unlock();
            };
        vTaskDelay(100 / portTICK_RATE_MS);
    }
}


[[noreturn]]void observing_request();

//char *buf[40];
//static time_t last_server_check_time = 0;

/*void read_server_time() {
	WAIT_FOR_NETWORK()  // just waits until network connects
	last_server_check_time = 0;
	while (last_server_check_time == 0) {
		Request *timeSynch = Request::new_get()
				->setUrl("coap://192.168.1.5/time")
				->setSession(coap_client)
				->Handler([](const uint8_t *data, unsigned int len) {
					cn_cbor_errback res;
					res.err = CN_CBOR_NO_ERROR;
					res.pos = 0;
					cn_cbor *item = cn_cbor_decode(data, len, &res);
					if (res.err == CN_CBOR_NO_ERROR) {
						last_server_check_time = item->v.uint;
					}
				})
				->Timeout(5);
		timeSynch->SendAndWait();
		Request::release(timeSynch);
		if (!last_server_check_time) {
			vTaskDelay(10 * 1000 / portTICK_RATE_MS);
		}
	}
}*/

ExpBackoff *observation_backOff = nullptr;

inline ExpBackoff *get_obs_backoff() {
    return new ExpBackoff(1440, 65535, 5000);
}

void queue_incomming_observable_data(const uint8_t *data, unsigned int len) {
    std::vector<device_message *> *items = device_message::from_binary_t((uint8_t *) data, len);
    //todo: match clientId
    hold_commands.lock();
    for (auto &item : *items) {
        cmd_queue.push_back(item);
    }
    hold_commands.unlock();
    delete items;
}

[[noreturn]] void observing_request() {

    MonitorMemory mon("observe");
    while (true) {
        WAIT_FOR_NETWORK();
        if (get_session() == nullptr) {
            vTaskDelay(1000 / portTICK_RATE_MS);
            continue;
        }
//        mon.update();
//        ESP_LOGI(TAG, "Observing ...");
        Request *obs_request = Request::new_get()
                ->setSession(get_session())
                ->setUrl(get_observation_resource_url())
                ->Handler(queue_incomming_observable_data)
                ->Observe(MY_ESTABLISH)
                ->OnError([](const char *errText, uint errNo) {
                              printf("error: %s(%d)\n", errText, (int) errNo);
                          }
                )
                ->Timeout(120, true); // resets timeout on receipt of some data
        // if a mechanism checks server availability, this should never end
        // some nack errors should be able to close the connection, like too many retries
        obs_request->SendAndWait();
        Request::release(obs_request);
        clear_session();
//        mon.update();

        if (observation_backOff == nullptr)
            observation_backOff = get_obs_backoff();

        uint16_t ms = observation_backOff->get_backoffValue();
        if (ms == 0) { // tired of retrying
            delete observation_backOff;
            observation_backOff = nullptr;
            ESP_LOGI(TAG, "observation faied and max retry count has been reached"
                          ", a net activity or switch on/off will be necessary"
            );
            //todo: must enter circuit-break pattern
            uint mins = 0;
            while (true) {
                vTaskDelay(1000 / portTICK_RATE_MS);
                mins++;
                if (network_activity_detection || mins > 300) // wait for 5min or switching activity
                    break;
            }
        } else {
            ESP_LOGI(TAG, "observation failure!!! retrying in %u secs", ms / 1000);
        }
        vTaskDelay(ms / portTICK_RATE_MS);
    }
}

static esp_err_t ir_rx_nec_code_check(ir_rx_nec_data_t nec_code) {

    if ((nec_code.addr1 != ((~nec_code.addr2) & (uint32_t) 0xff))) {
        return ESP_FAIL;
    }

    if ((nec_code.cmd1 != ((~nec_code.cmd2) & (uint32_t) 0xff))) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

[[noreturn]] void start_ir_monitoring_task() {
    // GPIO 12-15 are configured JTAG so they need this
    gpio_config_t io_conf = {
            GPIO_OUTPUT_PIN_SEL,
            GPIO_MODE_OUTPUT,
            GPIO_PULLUP_DISABLE,
            GPIO_PULLDOWN_DISABLE,
            GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_direction(PIN_IR_RHT, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_IR_LFT, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_IR_LFT, 1);
    gpio_set_level(PIN_IR_RHT, 0);

    ir_rx_nec_data_t ir_data;
    ir_rx_config_t ir_rx_config = {
            .io_num = PIN_IR_INP,
            .buf_len = 128
    };
    ir_rx_init(&ir_rx_config);
    int32_t old_cmd = -1;
    //MonitorMemory mon("start_general");
    while (true) {
        //mon.update();
        ir_data.val = 0;
        int len = ir_rx_recv_data(&ir_data, 1, 200 / portTICK_PERIOD_MS); //
        inProximity =
                (len > 0) &&
                (
                        (ir_data.addr1 == 0xAB && ir_data.cmd1 == 0xAB) ||
                        (ir_data.addr1 == 0xCD && ir_data.cmd1 == 0xCD)) &&
                (ESP_OK == ir_rx_nec_code_check(ir_data));
        if (inProximity || len == 0)
            old_cmd = -1;
        else if (len > 0) {
            if (ir_data.addr1 == 0xBE) { // 190 vol up/down and page up/down
                if (old_cmd != ir_data.cmd1) {
                    switch (ir_data.cmd1) {
                        case 0: //pageup light on
                            old_cmd = ir_data.cmd1;
                            light_switch.switch_on(true);
                            break;
                        case 1: //pagedown
                            old_cmd = ir_data.cmd1;
                            light_switch.switch_on(false);
                            break;
                        case 2: // vol up- backlight
                            old_cmd = ir_data.cmd1;
                            backLight_switch.switch_on(STATE_ON);
                            break;
                        case 3: //vol down
                            old_cmd = ir_data.cmd1;
                            backLight_switch.switch_on(STATE_OFF);
                            break;
                        case 12: // enter to smartConfig mode
                            reset_connection_data();
                            wifi_disconnect();
                            clear_session();
                            break;
                        default:
//                          printf("cmd(%d)\n", ir_data.cmd1);
                            break;
                    }
                }
            } else old_cmd = -1;
        }
    }
}

[[noreturn]] void start_switching_task() {
    light_switch.init(false); // this is necessary for morse to work
    backLight_switch.setStartDelay(1900);
    backLight_switch.init(STATE_OFF);
    unsigned long new_time;
    static int64_t old_time = 0;
    //MonitorMemory mon("start_switching");
    while (true) {
        //mon.update();
        new_time = esp_timer_get_time() / 1000;
        light_switch.check(inProximity, new_time);
        backLight_switch.check(inProximity, new_time);
        if (WIFI_IS_READY()) { // connected
            old_time = 0;
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }
        if (new_time - old_time < 5000) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }
        old_time = new_time; // ignore play_more time
        if (smartConfig_is_running())
            player.play_morse(".."); // configuring
        else if (wifi_is_running())
            player.play_morse("--"); // connecting
    }
}

[[noreturn]] void start_network_task() {
    //MonitorMemory mon("network");
    auto set_new_connection_data = [](user_pass_t *new_info) {
        puts("Saving connection data");
        memcpy(&connection_data, new_info, sizeof(connection_data));
        write_connection_data_to_flash(&connection_data);
        config_build_url_variables();
        clear_session();
    };
    int conn_retry = 0;
    while (true) {
        //mon.update();
        if (!wifi_is_running() && !smartConfig_is_running()) {
            if (strlen(connection_data.ssid) > 0) {
                if (wifi_connect(connection_data.ssid, connection_data.passwd, 60) == ESP_OK || ++conn_retry < 3) // if succeeded
                    continue;
            }
            conn_retry = 0;
            app_smart_config(set_new_connection_data); // may need to store connection info
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

//void find_partitions(void);
extern "C" void coap_example_client(void *p);

void app_main(void) {
    ESP_ERROR_CHECK(esp_netif_init())
    ESP_ERROR_CHECK(nvs_flash_init())
    ESP_ERROR_CHECK(esp_event_loop_create_default())
//	find_partitions();
    config_load_connection_data_from_flash(&connection_data);
    config_load_mfg_keys_from_factory();
    config_build_url_variables();

    ExecuteDelayedTask(start_switching_task, 0, 2, "light switching Task");
    ExecuteDelayedTask(start_ir_monitoring_task, 0, 3, "irCheck Task");
    ExecuteDelayedTask(start_network_task, 0, 4, "network Task");

    printf("Product : %s\n"
           "Serial  : %s\n"
           "Firmware: %s\n"
           "App : %s\n"
           "Identity: %s\n"
           "Report  : %s\n"
           "Watch  :  %s\n",
           device_keys.model,
           device_keys.serial,
           device_keys.firmware,
           device_keys.application,
           device_keys.identity,
           get_report_resource_url(),
           get_observation_resource_url()
    );
    char b[200];
    printf("security lib version(%s)\n", coap_string_tls_version(b, sizeof(b)));

    WAIT_FOR_NETWORK()
    while (!get_session()) { // waiting for coap session
        vTaskDelay(1000 / portTICK_RATE_MS);
    }

    ExecuteDelayedTask(outputs_loop, 0, 6, "poster Task");
    ExecuteDelayedTask(inputs_loop, 0, 4, "execute_cmds Task");
    ExecuteDelayedTask(observing_request, 0, 8, "observer Task");
}


