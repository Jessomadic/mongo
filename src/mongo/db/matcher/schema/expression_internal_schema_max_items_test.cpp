/**
 *    Copyright (C) 2018-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include <string>

#include <boost/move/utility_core.hpp>
#include <boost/optional/optional.hpp>

#include "mongo/bson/bsonmisc.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/db/exec/matcher/matcher.h"
#include "mongo/db/matcher/schema/expression_internal_schema_max_items.h"
#include "mongo/unittest/assert.h"
#include "mongo/unittest/death_test.h"
#include "mongo/unittest/framework.h"
#include "mongo/util/assert_util.h"

namespace mongo {

namespace {

TEST(InternalSchemaMaxItemsMatchExpression, RejectsNonArrayElements) {
    InternalSchemaMaxItemsMatchExpression maxItems("a"_sd, 1);

    ASSERT(!exec::matcher::matchesBSON(&maxItems, BSON("a" << BSONObj())));
    ASSERT(!exec::matcher::matchesBSON(&maxItems, BSON("a" << 1)));
    ASSERT(!exec::matcher::matchesBSON(&maxItems,
                                       BSON("a"
                                            << "string")));
}

TEST(InternalSchemaMaxItemsMatchExpression, RejectsArraysWithTooManyElements) {
    InternalSchemaMaxItemsMatchExpression maxItems("a"_sd, 0);

    ASSERT(!exec::matcher::matchesBSON(&maxItems, BSON("a" << BSON_ARRAY(1))));
    ASSERT(!exec::matcher::matchesBSON(&maxItems, BSON("a" << BSON_ARRAY(1 << 2))));
}

TEST(InternalSchemaMaxItemsMatchExpression, AcceptsArrayWithLessThanOrEqualToMaxElements) {
    InternalSchemaMaxItemsMatchExpression maxItems("a"_sd, 2);

    ASSERT(exec::matcher::matchesBSON(&maxItems, BSON("a" << BSON_ARRAY(5 << 6))));
    ASSERT(exec::matcher::matchesBSON(&maxItems, BSON("a" << BSON_ARRAY(5))));
}

TEST(InternalSchemaMaxItemsMatchExpression, MaxItemsZeroAllowsEmptyArrays) {
    InternalSchemaMaxItemsMatchExpression maxItems("a"_sd, 0);

    ASSERT(exec::matcher::matchesBSON(&maxItems, BSON("a" << BSONArray())));
}

TEST(InternalSchemaMaxItemsMatchExpression, NullArrayEntriesCountAsItems) {
    InternalSchemaMaxItemsMatchExpression maxItems("a"_sd, 2);

    ASSERT(exec::matcher::matchesBSON(&maxItems, BSON("a" << BSON_ARRAY(BSONNULL << 1))));
    ASSERT(!exec::matcher::matchesBSON(&maxItems, BSON("a" << BSON_ARRAY(BSONNULL << 1 << 2))));
}

TEST(InternalSchemaMaxItemsMatchExpression, NestedArraysAreNotUnwound) {
    InternalSchemaMaxItemsMatchExpression maxItems("a"_sd, 2);

    ASSERT(exec::matcher::matchesBSON(&maxItems, BSON("a" << BSON_ARRAY(BSON_ARRAY(1 << 2 << 3)))));
}

TEST(InternalSchemaMaxItemsMatchExpression, NestedArraysWorkWithDottedPaths) {
    InternalSchemaMaxItemsMatchExpression maxItems("a.b"_sd, 2);

    ASSERT(exec::matcher::matchesBSON(&maxItems, BSON("a" << BSON("b" << BSON_ARRAY(1)))));
    ASSERT(
        !exec::matcher::matchesBSON(&maxItems, BSON("a" << BSON("b" << BSON_ARRAY(1 << 2 << 3)))));
}

DEATH_TEST_REGEX(InternalSchemaMaxItemsMatchExpression,
                 GetChildFailsIndexGreaterThanZero,
                 "Tripwire assertion.*6400215") {
    InternalSchemaMaxItemsMatchExpression maxItems("a"_sd, 2);

    ASSERT_EQ(maxItems.numChildren(), 0);
    ASSERT_THROWS_CODE(maxItems.getChild(0), AssertionException, 6400215);
}

}  // namespace
}  // namespace mongo
