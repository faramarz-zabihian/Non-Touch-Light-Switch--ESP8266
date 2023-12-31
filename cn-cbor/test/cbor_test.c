/*
 * Copyright (c) 2015 SPUDlib authors.  See LICENSE file.
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "cn-cbor/cn-cbor.h"
#include "context.h"

#define CTEST_MAIN
#include "ctest.h"

#ifdef USE_CBOR_CONTEXT
cn_cbor_context *context;
#endif

int main(int argc, const char *argv[])
{
#ifdef USE_CBOR_CONTEXT
	context = CreateContext(-1);
#endif

	int i = ctest_main(argc, argv);
#ifdef USE_CBOR_CONTEXT
	if (IsContextEmpty(context) > 0) {
		i += 1;
		printf("MEMORY CONTEXT NOT EMPTY");
	}
#endif
	return i;
}

#ifdef USE_CBOR_CONTEXT
#define CONTEXT_NULL , context
#define CONTEXT_NULL_COMMA context,
#else
#define CONTEXT_NULL
#define CONTEXT_NULL_COMMA
#endif

typedef struct _buffer {
	size_t sz;
	unsigned char *ptr;
} buffer;

static bool parse_hex(char *inp, buffer *b)
{
	size_t len = strlen(inp);
	size_t i = 0;
	if (len % 2 != 0) {
		b->sz = -1;
		b->ptr = NULL;
		return false;
	}
	b->sz = len / 2;
	b->ptr = malloc(b->sz);
	for (i = 0; i < b->sz; i++) {
#ifdef _MSC_VER
		unsigned short iX;
		sscanf(inp + (2 * i), "%02hx", &iX);
		b->ptr[i] = (byte)iX;
#else
		sscanf(inp + (2 * i), "%02hhx", &b->ptr[i]);
#endif
	}
	return true;
}

CTEST(cbor, error)
{
	ASSERT_STR(cn_cbor_error_str[CN_CBOR_NO_ERROR], "CN_CBOR_NO_ERROR");
	ASSERT_STR(cn_cbor_error_str[CN_CBOR_ERR_OUT_OF_DATA], "CN_CBOR_ERR_OUT_OF_DATA");
	ASSERT_STR(cn_cbor_error_str[CN_CBOR_ERR_NOT_ALL_DATA_CONSUMED], "CN_CBOR_ERR_NOT_ALL_DATA_CONSUMED");
	ASSERT_STR(cn_cbor_error_str[CN_CBOR_ERR_ODD_SIZE_INDEF_MAP], "CN_CBOR_ERR_ODD_SIZE_INDEF_MAP");
	ASSERT_STR(cn_cbor_error_str[CN_CBOR_ERR_BREAK_OUTSIDE_INDEF], "CN_CBOR_ERR_BREAK_OUTSIDE_INDEF");
	ASSERT_STR(cn_cbor_error_str[CN_CBOR_ERR_MT_UNDEF_FOR_INDEF], "CN_CBOR_ERR_MT_UNDEF_FOR_INDEF");
	ASSERT_STR(cn_cbor_error_str[CN_CBOR_ERR_RESERVED_AI], "CN_CBOR_ERR_RESERVED_AI");
	ASSERT_STR(
		cn_cbor_error_str[CN_CBOR_ERR_WRONG_NESTING_IN_INDEF_STRING], "CN_CBOR_ERR_WRONG_NESTING_IN_INDEF_STRING");
	ASSERT_STR(cn_cbor_error_str[CN_CBOR_ERR_INVALID_PARAMETER], "CN_CBOR_ERR_INVALID_PARAMETER");
	ASSERT_STR(cn_cbor_error_str[CN_CBOR_ERR_OUT_OF_MEMORY], "CN_CBOR_ERR_OUT_OF_MEMORY");
	ASSERT_STR(cn_cbor_error_str[CN_CBOR_ERR_FLOAT_NOT_SUPPORTED], "CN_CBOR_ERR_FLOAT_NOT_SUPPORTED");
}

CTEST(cbor, parse)
{
	cn_cbor_errback err;
	char *tests[] = {
		"00",				   // 0
		"01",				   // 1
		"17",				   // 23
		"1818",				   // 24
		"190100",			   // 256
		"1a00010000",		   // 65536
		"1b0000000100000000",  // 4294967296
		"20",				   // -1
		"37",				   // -24
		"3818",				   // -25
		"390100",			   // -257
		"3a00010000",		   // -65537
		"3b0000000100000000",  // -4294967297
		"4161",				   // h"a"
		"6161",				   // "a"
		"80",				   // []
		"8100",				   // [0]
		"820102",			   // [1,2]
		"818100",			   // [[0]]
		"a1616100",			   // {"a":0}
		"d8184100",			   // tag
		"f4",				   // false
		"f5",				   // true
		"f6",				   // null
		"f7",				   // undefined
		"f8ff",				   // simple(255)
#ifndef CBOR_NO_FLOAT
		"f93c00",				   // 1.0
		"f9bc00",				   // -1.0
		"f903ff",				   // 6.097555160522461e-05
		"f90400",				   // 6.103515625e-05
		"f907ff",				   // 0.00012201070785522461
		"f90800",				   // 0.0001220703125
		"fa47800000",			   // 65536.0
		"fb3ff199999999999a",	   // 1.1
		"f97e00",				   // NaN
#endif							   /* CBOR_NO_FLOAT */
		"5f42010243030405ff",	   // (_ h'0102', h'030405')
		"7f61616161ff",			   // (_ "a", "a")
		"9fff",					   // [_ ]
		"9f9f9fffffff",			   // [_ [_ [_ ]]]
		"9f009f00ff00ff",		   // [_ 0, [_ 0], 0]
		"bf61610161629f0203ffff",  // {_ "a": 1, "b": [_ 2, 3]}
	};
	cn_cbor *cb;
	buffer b;
	size_t i;
	unsigned char encoded[1024];
	ssize_t enc_sz;
	ssize_t enc_sz2;

	for (i = 0; i < sizeof(tests) / sizeof(char *); i++) {
		ASSERT_TRUE(parse_hex(tests[i], &b));
		err.err = CN_CBOR_NO_ERROR;
		cb = cn_cbor_decode(b.ptr, b.sz CONTEXT_NULL, &err);
		// CTEST_LOG("%s: %s", tests[i], cn_cbor_error_str[err.err]);
		ASSERT_EQUAL(err.err, CN_CBOR_NO_ERROR);
		ASSERT_NOT_NULL(cb);

		enc_sz2 = cn_cbor_encoder_write(NULL, 0, sizeof(encoded), cb);
		enc_sz = cn_cbor_encoder_write(encoded, 0, sizeof(encoded), cb);
		ASSERT_EQUAL(enc_sz, enc_sz2);
		ASSERT_DATA(b.ptr, b.sz, encoded, enc_sz);
		free(b.ptr);
		cn_cbor_free(cb CONTEXT_NULL);
	}
}

CTEST(cbor, parse_normalize)
{
	cn_cbor_errback err;
	char *basic_tests[] = {
		"00",
		"00",  // 0
		"1800",
		"00",
		"1818",
		"1818",
		"190000",
		"00",
		"190018",
		"1818",
		"1a00000000",
		"00",
		"1b0000000000000000",
		"00",
		"20",
		"20",  // -1
		"3800",
		"20",
		"c600",
		"c600",	 // 6(0) (undefined tag)
		"d80600",
		"c600",
		"d9000600",
		"c600",
	};
	char *float_tests[] = {
		"fb3ff0000000000000", "f93c00",		 // 1.0
		"fbbff0000000000000", "f9bc00",		 // -1.0
		"fb40f86a0000000000", "fa47c35000",	 // 100000.0
		"fb7ff8000000000000", "f97e00",		 // NaN
		"fb3e70000000000000", "f90001",		 // 5.960464477539063e-08
		"fb3e78000000000000", "fa33c00000",	 //  8.940696716308594e-08
		"fb3e80000000000000", "f90002",		 // 1.1920928955078125e-07
	};
	cn_cbor *cb;
	buffer b, b2;
	size_t i;
	unsigned char encoded[1024];
	ssize_t enc_sz;
	ssize_t enc_sz2;

	for (i = 0; i < sizeof(basic_tests) / sizeof(char *); i += 2) {
		ASSERT_TRUE(parse_hex(basic_tests[i], &b));
		ASSERT_TRUE(parse_hex(basic_tests[i + 1], &b2));
		err.err = CN_CBOR_NO_ERROR;
		cb = cn_cbor_decode(b.ptr, b.sz CONTEXT_NULL, &err);
		CTEST_LOG("%s: %s", basic_tests[i], cn_cbor_error_str[err.err]);
		ASSERT_EQUAL(err.err, CN_CBOR_NO_ERROR);
		ASSERT_NOT_NULL(cb);

		enc_sz2 = cn_cbor_encoder_write(NULL, 0, sizeof(encoded), cb);
		enc_sz = cn_cbor_encoder_write(encoded, 0, sizeof(encoded), cb);
		ASSERT_EQUAL(enc_sz, enc_sz2);
		ASSERT_DATA(b2.ptr, b2.sz, encoded, enc_sz);
		free(b.ptr);
		free(b2.ptr);
		cn_cbor_free(cb CONTEXT_NULL);
	}

	for (i = 0; i < sizeof(float_tests) / sizeof(char *); i += 2) {
		ASSERT_TRUE(parse_hex(float_tests[i], &b));
		ASSERT_TRUE(parse_hex(float_tests[i + 1], &b2));
		err.err = CN_CBOR_NO_ERROR;
		cb = cn_cbor_decode(b.ptr, b.sz CONTEXT_NULL, &err);
		CTEST_LOG("%s: %s", float_tests[i], cn_cbor_error_str[err.err]);
#ifndef CBOR_NO_FLOAT
		ASSERT_EQUAL(err.err, CN_CBOR_NO_ERROR);
		ASSERT_NOT_NULL(cb);
#else  /* CBOR_NO_FLOAT */
		ASSERT_EQUAL(err.err, CN_CBOR_ERR_FLOAT_NOT_SUPPORTED);
		ASSERT_NULL(cb);
#endif /* CBOR_NO_FLOAT */

		/* enc_sz2 = cn_cbor_encoder_write(NULL, 0, sizeof(encoded), cb); */
		/* enc_sz = cn_cbor_encoder_write(encoded, 0, sizeof(encoded), cb); */
		/* ASSERT_EQUAL(enc_sz, enc_sz2); */
		/* ASSERT_DATA(b2.ptr, b2.sz, encoded, enc_sz); */
		free(b.ptr);
		free(b2.ptr);
		cn_cbor_free(cb CONTEXT_NULL);
	}
}

typedef struct _cbor_failure {
	char *hex;
	cn_cbor_error err;
} cbor_failure;

CTEST(cbor, fail)
{
	cn_cbor_errback err;
	cbor_failure tests[] = {
		{"81", CN_CBOR_ERR_OUT_OF_DATA},
		{"0000", CN_CBOR_ERR_NOT_ALL_DATA_CONSUMED},
		{"bf00ff", CN_CBOR_ERR_ODD_SIZE_INDEF_MAP},
		{"ff", CN_CBOR_ERR_BREAK_OUTSIDE_INDEF},
		{"1f", CN_CBOR_ERR_MT_UNDEF_FOR_INDEF},
		{"1c", CN_CBOR_ERR_RESERVED_AI},
		{"7f4100", CN_CBOR_ERR_WRONG_NESTING_IN_INDEF_STRING},
	};
	cn_cbor *cb;
	buffer b;
	size_t i;
	uint8_t buf[10];
	cn_cbor inv = {CN_CBOR_INVALID, 0, {0}, 0, NULL, NULL, NULL, NULL};

	ASSERT_EQUAL(-1, cn_cbor_encoder_write(buf, 0, sizeof(buf), &inv));

	for (i = 0; i < sizeof(tests) / sizeof(cbor_failure); i++) {
		ASSERT_TRUE(parse_hex(tests[i].hex, &b));
		cb = cn_cbor_decode(b.ptr, b.sz CONTEXT_NULL, &err);
		ASSERT_NULL(cb);
		ASSERT_EQUAL(err.err, tests[i].err);

		free(b.ptr);
		cn_cbor_free(cb CONTEXT_NULL);
	}
}

// Decoder loses float size information
CTEST(cbor, float)
{
#ifndef CBOR_NO_FLOAT
	cn_cbor_errback err;
	char *tests[] = {
		"f90001",	   // 5.960464477539063e-08
		"f9c400",	   // -4.0
		"fa47c35000",  // 100000.0
		"f97e00",	   // Half NaN, half beast
		"f9fc00",	   // -Inf
		"f97c00",	   // Inf
	};
	cn_cbor *cb;
	buffer b;
	size_t i;
	unsigned char encoded[1024];
	ssize_t enc_sz;
	ssize_t enc_sz2;

	for (i = 0; i < sizeof(tests) / sizeof(char *); i++) {
		ASSERT_TRUE(parse_hex(tests[i], &b));
		cb = cn_cbor_decode(b.ptr, b.sz CONTEXT_NULL, &err);
		ASSERT_NOT_NULL(cb);

		enc_sz2 = cn_cbor_encoder_write(NULL, 0, sizeof(encoded), cb);
		enc_sz = cn_cbor_encoder_write(encoded, 0, sizeof(encoded), cb);
		ASSERT_EQUAL(enc_sz, enc_sz2);
		ASSERT_DATA(b.ptr, b.sz, encoded, enc_sz);

		free(b.ptr);
		cn_cbor_free(cb CONTEXT_NULL);
	}
#endif /* CBOR_NO_FLOAT */
}

CTEST(cbor, getset)
{
	buffer b;
	cn_cbor *cb;
	cn_cbor *val;
	cn_cbor_errback err;

	ASSERT_TRUE(parse_hex("a40000436363630262626201616100", &b));
	cb = cn_cbor_decode(b.ptr, b.sz CONTEXT_NULL, &err);
	ASSERT_NOT_NULL(cb);
	val = cn_cbor_mapget_string(cb, "a");
	ASSERT_NOT_NULL(val);
	val = cn_cbor_mapget_string(cb, "bb");
	ASSERT_NOT_NULL(val);
	val = cn_cbor_mapget_string(cb, "ccc");
	ASSERT_NOT_NULL(val);
	val = cn_cbor_mapget_string(cb, "b");
	ASSERT_NULL(val);
	free(b.ptr);
	cn_cbor_free(cb CONTEXT_NULL);

	ASSERT_TRUE(parse_hex("a3616100006161206162", &b));
	cb = cn_cbor_decode(b.ptr, b.sz CONTEXT_NULL, &err);
	ASSERT_NOT_NULL(cb);
	val = cn_cbor_mapget_int(cb, 0);
	ASSERT_NOT_NULL(val);
	val = cn_cbor_mapget_int(cb, -1);
	ASSERT_NOT_NULL(val);
	val = cn_cbor_mapget_int(cb, 1);
	ASSERT_NULL(val);
	free(b.ptr);
	cn_cbor_free(cb CONTEXT_NULL);

	ASSERT_TRUE(parse_hex("8100", &b));
	cb = cn_cbor_decode(b.ptr, b.sz CONTEXT_NULL, &err);
	ASSERT_NOT_NULL(cb);
	val = cn_cbor_index(cb, 0);
	ASSERT_NOT_NULL(val);
	val = cn_cbor_index(cb, 1);
	ASSERT_NULL(val);
	val = cn_cbor_index(cb, -1);
	ASSERT_NULL(val);
	free(b.ptr);
	cn_cbor_free(cb CONTEXT_NULL);
}

CTEST(cbor, create)
{
	cn_cbor_errback err;
	const cn_cbor *val;
	const char *data = "abc";
	cn_cbor *cb_map = cn_cbor_map_create(CONTEXT_NULL_COMMA & err);
	cn_cbor *cb_int;
	cn_cbor *cb_data;
#ifndef CBOR_NO_FLOAT
	cn_cbor *cb_dbl;
#endif

	ASSERT_NOT_NULL(cb_map);
	ASSERT_TRUE(err.err == CN_CBOR_NO_ERROR);

	cb_int = cn_cbor_int_create(256 CONTEXT_NULL, &err);
	ASSERT_NOT_NULL(cb_int);
	ASSERT_TRUE(err.err == CN_CBOR_NO_ERROR);

	cb_data = cn_cbor_data_create((const uint8_t *)data, 4 CONTEXT_NULL, &err);
	ASSERT_NOT_NULL(cb_data);
	ASSERT_TRUE(err.err == CN_CBOR_NO_ERROR);

#ifndef CBOR_NO_FLOAT
	cb_dbl = cn_cbor_double_create(3.14159 CONTEXT_NULL, &err);
	ASSERT_NOT_NULL(cb_dbl);
	ASSERT_TRUE(err.err == CN_CBOR_NO_ERROR);
#endif

	cn_cbor_mapput_int(cb_map, 5, cb_int CONTEXT_NULL, &err);
	ASSERT_TRUE(err.err == CN_CBOR_NO_ERROR);
	ASSERT_TRUE(cb_map->length == 2);

	cn_cbor_mapput_int(cb_map, -7, cb_data CONTEXT_NULL, &err);
	ASSERT_TRUE(err.err == CN_CBOR_NO_ERROR);
	ASSERT_TRUE(cb_map->length == 4);

	cn_cbor_mapput_string(cb_map, "foo", cn_cbor_string_create(data CONTEXT_NULL, &err) CONTEXT_NULL, &err);
	ASSERT_TRUE(err.err == CN_CBOR_NO_ERROR);
	ASSERT_TRUE(cb_map->length == 6);

	cn_cbor_map_put(
		cb_map, cn_cbor_string_create("bar" CONTEXT_NULL, &err), cn_cbor_string_create("qux" CONTEXT_NULL, &err), &err);
	ASSERT_TRUE(err.err == CN_CBOR_NO_ERROR);
	ASSERT_TRUE(cb_map->length == 8);

#ifndef CBOR_NO_FLOAT
	cn_cbor_mapput_int(cb_map, 42, cb_dbl CONTEXT_NULL, &err);
	ASSERT_TRUE(err.err == CN_CBOR_NO_ERROR);
	ASSERT_TRUE(cb_map->length == 10);
#endif

	val = cn_cbor_mapget_int(cb_map, 5);
	ASSERT_NOT_NULL(val);
	ASSERT_TRUE(val->v.sint == 256);

	val = cn_cbor_mapget_int(cb_map, -7);
	ASSERT_NOT_NULL(val);
	ASSERT_STR(val->v.str, "abc");

#ifndef CBOR_NO_FLOAT
	val = cn_cbor_mapget_int(cb_map, 42);
	ASSERT_NOT_NULL(val);
	ASSERT_TRUE(val->v.dbl > 3.14 && val->v.dbl < 3.15);
#endif

	cn_cbor_free(cb_map CONTEXT_NULL);
}

CTEST(cbor, map_errors)
{
	cn_cbor_errback err;
	cn_cbor *ci;
	ci = cn_cbor_int_create(65536, CONTEXT_NULL_COMMA & err);
	cn_cbor_mapput_int(ci, -5, NULL, CONTEXT_NULL_COMMA & err);
	ASSERT_EQUAL(err.err, CN_CBOR_ERR_INVALID_PARAMETER);
	cn_cbor_mapput_string(ci, "foo", NULL, CONTEXT_NULL_COMMA & err);
	ASSERT_EQUAL(err.err, CN_CBOR_ERR_INVALID_PARAMETER);
	cn_cbor_map_put(ci, NULL, NULL, &err);
	ASSERT_EQUAL(err.err, CN_CBOR_ERR_INVALID_PARAMETER);
	cn_cbor_free(ci CONTEXT_NULL);
}

CTEST(cbor, array)
{
	cn_cbor_errback err;
	cn_cbor *a = cn_cbor_array_create(CONTEXT_NULL_COMMA & err);
	ASSERT_NOT_NULL(a);
	ASSERT_TRUE(err.err == CN_CBOR_NO_ERROR);
	ASSERT_EQUAL(a->length, 0);

	cn_cbor_array_append(a, cn_cbor_int_create(256, CONTEXT_NULL_COMMA & err), &err);
	ASSERT_TRUE(err.err == CN_CBOR_NO_ERROR);
	ASSERT_EQUAL(a->length, 1);

	cn_cbor_array_append(a, cn_cbor_string_create("five", CONTEXT_NULL_COMMA & err), &err);
	ASSERT_TRUE(err.err == CN_CBOR_NO_ERROR);
	ASSERT_EQUAL(a->length, 2);
	cn_cbor_free(a CONTEXT_NULL);
}

CTEST(cbor, array_errors)
{
	cn_cbor_errback err;
	cn_cbor *ci = cn_cbor_int_create(12, CONTEXT_NULL_COMMA & err);
	cn_cbor_array_append(NULL, ci, &err);
	ASSERT_EQUAL(err.err, CN_CBOR_ERR_INVALID_PARAMETER);
	cn_cbor_array_append(ci, NULL, &err);
	ASSERT_EQUAL(err.err, CN_CBOR_ERR_INVALID_PARAMETER);
	cn_cbor_free(ci CONTEXT_NULL);
}

CTEST(cbor, create_encode)
{
	cn_cbor *map;
	cn_cbor *cdata;
	char data[] = "data";
	unsigned char encoded[1024];
	ssize_t enc_sz;

	map = cn_cbor_map_create(CONTEXT_NULL_COMMA NULL);
	ASSERT_NOT_NULL(map);

	cdata = cn_cbor_data_create((uint8_t *)data, sizeof(data) - 1, CONTEXT_NULL_COMMA NULL);
	ASSERT_NOT_NULL(cdata);

	ASSERT_TRUE(cn_cbor_mapput_int(map, 0, cdata, CONTEXT_NULL_COMMA NULL));
	enc_sz = cn_cbor_encoder_write(NULL, 0, sizeof(encoded), map);
	ASSERT_EQUAL(7, enc_sz);
	enc_sz = cn_cbor_encoder_write(encoded, 0, sizeof(encoded), map);
	ASSERT_EQUAL(7, enc_sz);
	cn_cbor_free(map CONTEXT_NULL);
}

CTEST(cbor, fail2)
{
	cn_cbor_errback err;
	cbor_failure tests[] = {
		//  *  End of input in a head:
		{"18", CN_CBOR_ERR_OUT_OF_DATA},
		{"19", CN_CBOR_ERR_OUT_OF_DATA},
		{"1a", CN_CBOR_ERR_OUT_OF_DATA},
		{"1b", CN_CBOR_ERR_OUT_OF_DATA},
		{"1901", CN_CBOR_ERR_OUT_OF_DATA},
		{"1a0102", CN_CBOR_ERR_OUT_OF_DATA},
		{"1b01020304050607", CN_CBOR_ERR_OUT_OF_DATA},
		{"38", CN_CBOR_ERR_OUT_OF_DATA},
		{"58", CN_CBOR_ERR_OUT_OF_DATA},
		{"78", CN_CBOR_ERR_OUT_OF_DATA},
		{"98", CN_CBOR_ERR_OUT_OF_DATA},
		{"9a01ff00", CN_CBOR_ERR_OUT_OF_DATA},
		{"b8", CN_CBOR_ERR_OUT_OF_DATA},
		{"d8", CN_CBOR_ERR_OUT_OF_DATA},
		{"f8", CN_CBOR_ERR_OUT_OF_DATA},
		{"f900", CN_CBOR_ERR_OUT_OF_DATA},
		{"fa0000", CN_CBOR_ERR_OUT_OF_DATA},
		{"fb000000", CN_CBOR_ERR_OUT_OF_DATA},

		//   *  Definite length strings with short data:
		{"41", CN_CBOR_ERR_OUT_OF_DATA},
		{"61", CN_CBOR_ERR_OUT_OF_DATA},
		{"5affffffff00", CN_CBOR_ERR_OUT_OF_DATA},
		{"5bffffffffffffffff010203", CN_CBOR_ERR_OUT_OF_DATA},
		{"7affffffff00", CN_CBOR_ERR_OUT_OF_DATA},
		{"7b7fffffffffffffff010203", CN_CBOR_ERR_OUT_OF_DATA},

		//   *  Definite length maps and arrays not closed with enough items:
		{"81", CN_CBOR_ERR_OUT_OF_DATA},
		{"818181818181818181", CN_CBOR_ERR_OUT_OF_DATA},
		{"8200", CN_CBOR_ERR_OUT_OF_DATA},
		{"a1", CN_CBOR_ERR_OUT_OF_DATA},
		{"a20102", CN_CBOR_ERR_OUT_OF_DATA},
		{"a100", CN_CBOR_ERR_OUT_OF_DATA},
		{"a2000000", CN_CBOR_ERR_OUT_OF_DATA},

		//   *  Tag number not followed by tag content:
		{"c0", CN_CBOR_ERR_OUT_OF_DATA},

		//   *  Indefinite length strings not closed by a break stop code:
		{"5f4100", CN_CBOR_ERR_OUT_OF_DATA}, {"7f6100", CN_CBOR_ERR_OUT_OF_DATA},

		//   *  Indefinite length maps and arrays not closed by a break stop code:
		{"9f", CN_CBOR_ERR_OUT_OF_DATA}, {"9f0102", CN_CBOR_ERR_OUT_OF_DATA}, {"bf", CN_CBOR_ERR_OUT_OF_DATA},
		{"bf01020102", CN_CBOR_ERR_OUT_OF_DATA}, {"819f", CN_CBOR_ERR_OUT_OF_DATA}, {"9f8000", CN_CBOR_ERR_OUT_OF_DATA},
		{"9f9f9f9f9fffffffff", CN_CBOR_ERR_OUT_OF_DATA}, {"9f819f819f9fffffff", CN_CBOR_ERR_OUT_OF_DATA},

		//   *  Reserved additional information values:
		{"1c", CN_CBOR_ERR_RESERVED_AI},
		{"1d", CN_CBOR_ERR_RESERVED_AI},
		{"1e", CN_CBOR_ERR_RESERVED_AI},
		{"3c", CN_CBOR_ERR_RESERVED_AI},
		{"3d", CN_CBOR_ERR_RESERVED_AI},
		{"3e", CN_CBOR_ERR_RESERVED_AI},
		{"5c", CN_CBOR_ERR_RESERVED_AI},
		{"5d", CN_CBOR_ERR_RESERVED_AI},
		{"5e", CN_CBOR_ERR_RESERVED_AI},
		{"7c", CN_CBOR_ERR_RESERVED_AI},
		{"7d", CN_CBOR_ERR_RESERVED_AI},
		{"7e", CN_CBOR_ERR_RESERVED_AI},
		{"9c", CN_CBOR_ERR_RESERVED_AI},
		{"9d", CN_CBOR_ERR_RESERVED_AI},
		{"9e", CN_CBOR_ERR_RESERVED_AI},
		{"bc", CN_CBOR_ERR_RESERVED_AI},
		{"bd", CN_CBOR_ERR_RESERVED_AI},
		{"be", CN_CBOR_ERR_RESERVED_AI},
		{"dc", CN_CBOR_ERR_RESERVED_AI},
		{"dd", CN_CBOR_ERR_RESERVED_AI},
		{"de", CN_CBOR_ERR_RESERVED_AI},
		{"fc", CN_CBOR_ERR_RESERVED_AI},
		{"fd", CN_CBOR_ERR_RESERVED_AI},
		{"fe", CN_CBOR_ERR_RESERVED_AI},

		//   *  Reserved two-byte encodings of simple types:
#if 0
		{"f800", CN_CBOR_ERR_RESERVED_AI},
		{"f801", CN_CBOR_ERR_RESERVED_AI},
		{"f818", CN_CBOR_ERR_RESERVED_AI},
		{"f81f", CN_CBOR_ERR_RESERVED_AI},
#endif
		
		//   *  Indefinite length string chunks not of the correct type:
		{"5f00ff", CN_CBOR_ERR_WRONG_NESTING_IN_INDEF_STRING},
		{"5f21ff", CN_CBOR_ERR_WRONG_NESTING_IN_INDEF_STRING},
		{"5f6100ff", CN_CBOR_ERR_WRONG_NESTING_IN_INDEF_STRING},
		{"5f80ff", CN_CBOR_ERR_WRONG_NESTING_IN_INDEF_STRING},
		{"5fa0ff", CN_CBOR_ERR_WRONG_NESTING_IN_INDEF_STRING},
		{"5fc000ff", CN_CBOR_ERR_WRONG_NESTING_IN_INDEF_STRING},
		{"5fe0ff", CN_CBOR_ERR_WRONG_NESTING_IN_INDEF_STRING},
		{"7f4100ff", CN_CBOR_ERR_WRONG_NESTING_IN_INDEF_STRING},

		//   *  Indefinite length string chunks not definite length:
		{"5f5f4100ffff", CN_CBOR_ERR_WRONG_NESTING_IN_INDEF_STRING},
		{"7f7f6100ffff", CN_CBOR_ERR_WRONG_NESTING_IN_INDEF_STRING},

		//   *  Break occurring on its own outside of an indefinite length item:
		{"ff", CN_CBOR_ERR_BREAK_OUTSIDE_INDEF},

		//   *  Break occurring in a definite length array or map or a tag:
		{"81ff", CN_CBOR_ERR_BREAK_OUTSIDE_INDEF},
		{"8200ff", CN_CBOR_ERR_BREAK_OUTSIDE_INDEF},
		{"a1ff", CN_CBOR_ERR_BREAK_OUTSIDE_INDEF},
		{"a1ff00", CN_CBOR_ERR_BREAK_OUTSIDE_INDEF},
		{"a100ff", CN_CBOR_ERR_BREAK_OUTSIDE_INDEF},
		{"a20000ff", CN_CBOR_ERR_BREAK_OUTSIDE_INDEF},
		{"9f81ff", CN_CBOR_ERR_BREAK_OUTSIDE_INDEF},
		{"9f829f819f9fffffffff", CN_CBOR_ERR_BREAK_OUTSIDE_INDEF},

		//   *  Break in indefinite length map would lead to odd number of items (break in a value position):
		{"bf00ff", CN_CBOR_ERR_ODD_SIZE_INDEF_MAP},
		{"bf000000ff", CN_CBOR_ERR_ODD_SIZE_INDEF_MAP},

		//   *  Major type 0, 1, 6 with additional information 31:
		{"1f", CN_CBOR_ERR_MT_UNDEF_FOR_INDEF},
		{"3f", CN_CBOR_ERR_MT_UNDEF_FOR_INDEF},
		{"df", CN_CBOR_ERR_OUT_OF_DATA}
	};
	cn_cbor *cb;
	buffer b;
	size_t i;
	uint8_t buf[10];
	cn_cbor inv = {CN_CBOR_INVALID, 0, {0}, 0, NULL, NULL, NULL, NULL};

	ASSERT_EQUAL(-1, cn_cbor_encoder_write(buf, 0, sizeof(buf), &inv));

	for (i = 0; i < sizeof(tests) / sizeof(cbor_failure); i++) {
		ASSERT_TRUE(parse_hex(tests[i].hex, &b));
		cb = cn_cbor_decode(b.ptr, b.sz CONTEXT_NULL, &err);
		// CTEST_LOG("%s: %s %s", tests[i].hex, cn_cbor_error_str[tests[i].err], cn_cbor_error_str[err.err]);
		ASSERT_NULL(cb);
		ASSERT_EQUAL(err.err, tests[i].err);

		free(b.ptr);
		cn_cbor_free(cb CONTEXT_NULL);
	}
}
