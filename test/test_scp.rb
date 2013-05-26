#!/usr/bin/env ruby
# encoding: UTF-8

# Ubuntu does not accept arguments to ruby when called using env. To get warnings to show up the -w options is
# required. That can be set in the RUBYOPT environment variable.
# export RUBYOPT=-w

$VERBOSE = true

$: << File.join(File.dirname(__FILE__), "../lib")
$: << File.join(File.dirname(__FILE__), "../ext")

require 'test/unit'
require 'oj'
require 'pp'

$json = %{{
  "array": [
    {
      "num"   : 3,
      "string": "message",
      "hash"  : {
        "h2"  : {
          "a" : [ 1, 2, 3 ]
        }
      }
    }
  ],
  "boolean" : true
}}

class AllHandler < Oj::ScHandler
  attr_accessor :calls

  def initialize()
    @calls = []
  end

  def hash_start()
    @calls << [:hash_start]
  end

  def hash_end()
    @calls << [:hash_end]
  end

  def array_start()
    @calls << [:array_start]
  end

  def array_end()
    @calls << [:array_end]
  end

  def add_value(value)
    @calls << [:add_value, value]
  end

end # AllHandler

class SajTest < ::Test::Unit::TestCase

  def test_nil
    handler = AllHandler.new()
    json = %{null}
    Oj.saj_parse(handler, json)
    assert_equal([[:add_value, nil]], handler.calls)
  end

  def test_true
    handler = AllHandler.new()
    json = %{true}
    Oj.saj_parse(handler, json)
    assert_equal([[:add_value, true]], handler.calls)
  end

  def test_false
    handler = AllHandler.new()
    json = %{false}
    Oj.saj_parse(handler, json)
    assert_equal([[:add_value, false]], handler.calls)
  end

  def test_string
    handler = AllHandler.new()
    json = %{"a string"}
    Oj.saj_parse(handler, json)
    assert_equal([[:add_value, 'a string']], handler.calls)
  end

  def test_fixnum
    handler = AllHandler.new()
    json = %{12345}
    Oj.saj_parse(handler, json)
    assert_equal([[:add_value, 12345]], handler.calls)
  end

  def test_float
    handler = AllHandler.new()
    json = %{12345.6789}
    Oj.saj_parse(handler, json)
    assert_equal([[:add_value, 12345.6789]], handler.calls)
  end

  def test_float_exp
    handler = AllHandler.new()
    json = %{12345.6789e7}
    Oj.saj_parse(handler, json)
    assert_equal(1, handler.calls.size)
    assert_equal(:add_value, handler.calls[0][0])
    assert_equal((12345.6789e7 * 10000).to_i, (handler.calls[0][1] * 10000).to_i)
  end

  def test_array_empty
    handler = AllHandler.new()
    json = %{[]}
    Oj.saj_parse(handler, json)
    assert_equal([[:array_start],
                  [:array_end]], handler.calls)
  end

  def test_array
    handler = AllHandler.new()
    json = %{[true,false]}
    Oj.saj_parse(handler, json)
    assert_equal([[:array_start],
                  [:add_value, true],
                  [:add_value, false],
                  [:array_end]], handler.calls)
  end

  def test_hash_empty
    handler = AllHandler.new()
    json = %{{}}
    Oj.saj_parse(handler, json)
    assert_equal([[:hash_start],
                  [:hash_end]], handler.calls)
  end

  def test_hash
    handler = AllHandler.new()
    json = %{{"one":true,"two":false}}
    Oj.saj_parse(handler, json)
    assert_equal([[:hash_start],
                  [:add_value, "one"],
                  [:add_value, true],
                  [:add_value, "two"],
                  [:add_value, false],
                  [:hash_end]], handler.calls)
  end

  def test_full
    handler = AllHandler.new()
    Oj.saj_parse(handler, $json)
    assert_equal([[:hash_start],
                  [:add_value, 'array'],
                  [:array_start],
                  [:hash_start],
                  [:add_value, 'num'],
                  [:add_value, 3],
                  [:add_value, 'string'],
                  [:add_value, 'message'],
                  [:add_value, 'hash'],
                  [:hash_start],
                  [:add_value, 'h2'],
                  [:hash_start],
                  [:add_value, 'a'],
                  [:array_start],
                  [:add_value, 1],
                  [:add_value, 2],
                  [:add_value, 3],
                  [:array_end],
                  [:hash_end],
                  [:hash_end],
                  [:hash_end],
                  [:array_end],
                  [:add_value, 'boolean'],
                  [:add_value, true],
                  [:hash_end]], handler.calls)
  end

  def test_fixnum_bad
    handler = AllHandler.new()
    json = %{12345xyz}
    begin
      Oj.saj_parse(handler, json)
    rescue Exception => e
      assert_equal("unexpected character at line 1, column 6 [parse.c:473]", e.message)
    end
  end

end
