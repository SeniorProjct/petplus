/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "BridgingTest.h"

namespace facebook::react {

using namespace std::literals;

TEST_F(BridgingTest, jsiTest) {
  jsi::Value value = true;
  jsi::Value string = jsi::String::createFromAscii(rt, "hello");
  jsi::Value object = jsi::Object(rt);
  jsi::Value array = jsi::Array::createWithElements(rt, value, object);
  jsi::Value func = function("() => {}");

  // The bridging mechanism needs to know how to copy and downcast values.
  EXPECT_NO_THROW(bridging::fromJs<jsi::Value>(rt, value, invoker));
  EXPECT_NO_THROW(bridging::fromJs<jsi::String>(rt, string, invoker));
  EXPECT_NO_THROW(bridging::fromJs<jsi::Object>(rt, object, invoker));
  EXPECT_NO_THROW(bridging::fromJs<jsi::Array>(rt, array, invoker));
  EXPECT_NO_THROW(bridging::fromJs<jsi::Function>(rt, func, invoker));

  // Should throw when attempting an invalid cast.
  EXPECT_JSI_THROW(bridging::fromJs<jsi::Object>(rt, value, invoker));
  EXPECT_JSI_THROW(bridging::fromJs<jsi::String>(rt, array, invoker));
  EXPECT_JSI_THROW(bridging::fromJs<jsi::Array>(rt, object, invoker));
  EXPECT_JSI_THROW(bridging::fromJs<jsi::Array>(rt, string, invoker));
  EXPECT_JSI_THROW(bridging::fromJs<jsi::Array>(rt, func, invoker));

  // Should be able to generically no-op convert JSI.
  EXPECT_NO_THROW(bridging::toJs(rt, value, invoker));
  EXPECT_NO_THROW(bridging::toJs(rt, string.asString(rt), invoker));
  EXPECT_NO_THROW(bridging::toJs(rt, object.asObject(rt), invoker));
  EXPECT_NO_THROW(bridging::toJs(rt, array.asObject(rt).asArray(rt), invoker));
  EXPECT_NO_THROW(
      bridging::toJs(rt, func.asObject(rt).asFunction(rt), invoker));
}

TEST_F(BridgingTest, boolTest) {
  EXPECT_TRUE(bridging::fromJs<bool>(rt, jsi::Value(true), invoker));
  EXPECT_FALSE(bridging::fromJs<bool>(rt, jsi::Value(false), invoker));
  EXPECT_JSI_THROW(bridging::fromJs<bool>(rt, jsi::Value(1), invoker));

  EXPECT_TRUE(bridging::toJs(rt, true).asBool());
  EXPECT_FALSE(bridging::toJs(rt, false).asBool());
}

TEST_F(BridgingTest, numberTest) {
  EXPECT_EQ(1, bridging::fromJs<int>(rt, jsi::Value(1), invoker));
  EXPECT_FLOAT_EQ(1.2f, bridging::fromJs<float>(rt, jsi::Value(1.2), invoker));
  EXPECT_DOUBLE_EQ(1.2, bridging::fromJs<double>(rt, jsi::Value(1.2), invoker));
  EXPECT_JSI_THROW(bridging::fromJs<double>(rt, jsi::Value(true), invoker));

  EXPECT_EQ(1, static_cast<int>(bridging::toJs(rt, 1).asNumber()));
  EXPECT_FLOAT_EQ(
      1.2f, static_cast<float>(bridging::toJs(rt, 1.2f).asNumber()));
  EXPECT_DOUBLE_EQ(1.2, bridging::toJs(rt, 1.2).asNumber());
}

TEST_F(BridgingTest, stringTest) {
  auto string = jsi::String::createFromAscii(rt, "hello");

  EXPECT_EQ("hello"s, bridging::fromJs<std::string>(rt, string, invoker));
  EXPECT_JSI_THROW(bridging::fromJs<std::string>(rt, jsi::Value(1), invoker));

  EXPECT_TRUE(
      jsi::String::strictEquals(rt, string, bridging::toJs(rt, "hello")));
  EXPECT_TRUE(
      jsi::String::strictEquals(rt, string, bridging::toJs(rt, "hello"s)));
  EXPECT_TRUE(
      jsi::String::strictEquals(rt, string, bridging::toJs(rt, "hello"sv)));
}

TEST_F(BridgingTest, objectTest) {
  auto object = jsi::Object(rt);
  object.setProperty(rt, "foo", "bar");

  auto omap =
      bridging::fromJs<std::map<std::string, std::string>>(rt, object, invoker);
  auto umap = bridging::fromJs<std::unordered_map<std::string, std::string>>(
      rt, object, invoker);
  auto bmap = bridging::fromJs<butter::map<std::string, std::string>>(
      rt, object, invoker);

  EXPECT_EQ(1, omap.size());
  EXPECT_EQ(1, umap.size());
  EXPECT_EQ(1, bmap.size());
  EXPECT_EQ("bar"s, omap["foo"]);
  EXPECT_EQ("bar"s, umap["foo"]);
  EXPECT_EQ("bar"s, bmap["foo"]);

  EXPECT_EQ(
      "bar"s,
      bridging::toJs(rt, omap, invoker)
          .getProperty(rt, "foo")
          .asString(rt)
          .utf8(rt));
  EXPECT_EQ(
      "bar"s,
      bridging::toJs(rt, umap, invoker)
          .getProperty(rt, "foo")
          .asString(rt)
          .utf8(rt));
  EXPECT_EQ(
      "bar"s,
      bridging::toJs(rt, bmap, invoker)
          .getProperty(rt, "foo")
          .asString(rt)
          .utf8(rt));
}

TEST_F(BridgingTest, hostObjectTest) {
  struct TestHostObject : public jsi::HostObject {
    jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &name) override {
      if (name.utf8(rt) == "test") {
        return jsi::Value(1);
      }
      return jsi::Value::undefined();
    }
  };

  auto hostobject = std::make_shared<TestHostObject>();
  auto object = bridging::toJs(rt, hostobject);

  EXPECT_EQ(1, object.getProperty(rt, "test").asNumber());
  EXPECT_EQ(
      hostobject, bridging::fromJs<decltype(hostobject)>(rt, object, invoker));
}

TEST_F(BridgingTest, weakbjectTest) {
  auto object = jsi::Object(rt);
  auto weakobject = jsi::WeakObject(rt, object);

  EXPECT_TRUE(jsi::Object::strictEquals(
      rt,
      object,
      bridging::fromJs<jsi::WeakObject>(rt, object, invoker)
          .lock(rt)
          .asObject(rt)));

  EXPECT_TRUE(jsi::Object::strictEquals(
      rt, object, bridging::toJs(rt, weakobject).asObject(rt)));
}

TEST_F(BridgingTest, arrayTest) {
  auto vec = std::vector({"foo"s, "bar"s});
  auto array = jsi::Array::createWithElements(rt, "foo", "bar");

  EXPECT_EQ(
      vec, bridging::fromJs<std::vector<std::string>>(rt, array, invoker));

  EXPECT_EQ(vec.size(), bridging::toJs(rt, vec, invoker).size(rt));
  for (size_t i = 0; i < vec.size(); i++) {
    EXPECT_EQ(
        vec[i],
        bridging::toJs(rt, vec, invoker)
            .getValueAtIndex(rt, i)
            .asString(rt)
            .utf8(rt));
  }

  EXPECT_EQ(2, bridging::toJs(rt, std::make_pair(1, "2"), invoker).size(rt));
  EXPECT_EQ(2, bridging::toJs(rt, std::make_tuple(1, "2"), invoker).size(rt));
  EXPECT_EQ(2, bridging::toJs(rt, std::array<int, 2>{1, 2}, invoker).size(rt));
  EXPECT_EQ(2, bridging::toJs(rt, std::deque<int>{1, 2}, invoker).size(rt));
  EXPECT_EQ(2, bridging::toJs(rt, std::list<int>{1, 2}, invoker).size(rt));
  EXPECT_EQ(
      2,
      bridging::toJs(rt, std::initializer_list<int>{1, 2}, invoker).size(rt));
}

TEST_F(BridgingTest, functionTest) {
  auto object = jsi::Object(rt);
  object.setProperty(rt, "foo", "bar");

  auto lambda = [](std::map<std::string, std::string> map, std::string key) {
    return map[key];
  };

  auto func = bridging::toJs(rt, lambda, invoker);

  EXPECT_EQ(
      "bar"s,
      func.call(rt, object, jsi::String::createFromAscii(rt, "foo"))
          .asString(rt)
          .utf8(rt));

  // Should throw if not enough arguments are passed or are the wrong types.
  EXPECT_JSI_THROW(func.call(rt, object));
  EXPECT_JSI_THROW(func.call(rt, object, jsi::Value(1)));

  // Test with non-capturing lambda converted to function pointer.
  func = bridging::toJs(rt, +lambda, invoker);

  EXPECT_EQ(
      "bar"s,
      func.call(rt, object, jsi::String::createFromAscii(rt, "foo"))
          .asString(rt)
          .utf8(rt));
}

TEST_F(BridgingTest, syncCallbackTest) {
  auto fn = function("(a, b) => a + b");
  auto cb = bridging::fromJs<SyncCallback<std::string(std::string, int)>>(
      rt, fn, invoker);
  auto foo = "foo"s;

  EXPECT_EQ("foo1"s, cb(foo, 1)); // Tests lvalue string
  EXPECT_EQ("bar2", cb("bar", 2)); // Tests rvalue C string
  EXPECT_TRUE(fn.isFunction(rt)); // Ensure the function wasn't invalidated.
}

TEST_F(BridgingTest, asyncCallbackTest) {
  std::string output;

  auto func = std::function<void(std::string)>([&](auto str) { output = str; });

  auto cb = bridging::fromJs<AsyncCallback<decltype(func), std::string>>(
      rt, function("(func, str) => func(str)"), invoker);

  cb(func, "hello");

  flushQueue(); // Run scheduled async work

  EXPECT_EQ("hello"s, output);
}

TEST_F(BridgingTest, promiseTest) {
  auto func = function(
      "(promise, obj) => {"
      "  promise.then("
      "    (res) => { obj.res = res; },"
      "    (err) => { obj.err = err; }"
      "  )"
      "}");

  auto promise = AsyncPromise<std::vector<std::string>>(rt, invoker);
  auto output = jsi::Object(rt);

  func.call(rt, bridging::toJs(rt, promise, invoker), output);
  promise.resolve({"foo"s, "bar"s});
  flushQueue();

  EXPECT_EQ(1, output.getPropertyNames(rt).size(rt));
  EXPECT_EQ(2, output.getProperty(rt, "res").asObject(rt).asArray(rt).size(rt));
  EXPECT_NO_THROW(promise.resolve({"ignored"}));
  EXPECT_NO_THROW(promise.reject("ignored"));

  promise = AsyncPromise<std::vector<std::string>>(rt, invoker);
  output = jsi::Object(rt);

  func.call(rt, bridging::toJs(rt, promise, invoker), output);
  promise.reject("fail");
  flushQueue();

  EXPECT_EQ(1, output.getPropertyNames(rt).size(rt));
  EXPECT_EQ(
      "fail"s,
      output.getProperty(rt, "err")
          .asObject(rt)
          .getProperty(rt, "message")
          .asString(rt)
          .utf8(rt));
  EXPECT_NO_THROW(promise.resolve({"ignored"}));
  EXPECT_NO_THROW(promise.reject("ignored"));
}

TEST_F(BridgingTest, optionalTest) {
  EXPECT_EQ(
      1, bridging::fromJs<std::optional<int>>(rt, jsi::Value(1), invoker));
  EXPECT_FALSE(
      bridging::fromJs<std::optional<int>>(rt, jsi::Value::undefined(), invoker)
          .has_value());
  EXPECT_FALSE(
      bridging::fromJs<std::optional<int>>(rt, jsi::Value::null(), invoker)
          .has_value());

  EXPECT_TRUE(bridging::toJs(rt, std::optional<int>(), invoker).isNull());
  EXPECT_EQ(1, bridging::toJs(rt, std::optional<int>(1), invoker).asNumber());
}

TEST_F(BridgingTest, pointerTest) {
  auto str = "hi"s;
  auto unique = std::make_unique<std::string>(str);
  auto shared = std::make_shared<std::string>(str);
  auto weak = std::weak_ptr(shared);

  EXPECT_EQ(str, bridging::toJs(rt, unique, invoker).asString(rt).utf8(rt));
  EXPECT_EQ(str, bridging::toJs(rt, shared, invoker).asString(rt).utf8(rt));
  EXPECT_EQ(str, bridging::toJs(rt, weak, invoker).asString(rt).utf8(rt));

  shared.reset();

  EXPECT_TRUE(bridging::toJs(rt, weak, invoker).isNull());
}

} // namespace facebook::react
