module Oj
  # A SAX style parse handler for JSON hence the acronym SAJ for Simple API for
  # JSON. The Oj::SajKey handler class should be subclassed and then used with
  # the Oj.sajkey_parse() method. The SajKey methods will then be called as the
  # file is parsed.
  #
  # @example
  # 
  #  require 'oj'
  #
  #  class MySaj < ::Oj::SajKey
  #    def initialize()
  #      @hash_cnt = 0
  #    end
  #
  #    def start_hash(key)
  #      @hash_cnt += 1
  #    end
  #  end
  #
  #  cnt = MySaj.new()
  #  File.open('any.xml', 'r') do |f|
  #    Oj.saj_parse(cnt, f)
  #  end
  #
  # To make the desired methods active while parsing the desired method should
  # be made public in the subclasses. If the methods remain private they will
  # not be called during parsing.
  #
  #    def hash_start(key); end
  #    def hash_end(key); end
  #    def array_start(key); end
  #    def array_end(key); end
  #    def add_value(value, key); end
  #    def error(message, line, column); end
  #
  class SajKey
    # Create a new instance of the Saj handler class.
    def initialize()
    end

    # To make the desired methods active while parsing the desired method should
    # be made public in the subclasses. If the methods remain private they will
    # not be called during parsing.
    private

    def hash_start(key)
    end

    def hash_end(key)
    end

    def array_start(key)
    end

    def array_end(key)
    end

    def add_value(value, key)
    end

    def error(message, line, column)
    end
    
  end # SajKey
end # Oj
