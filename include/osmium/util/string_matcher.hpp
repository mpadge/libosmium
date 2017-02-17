#ifndef OSMIUM_UTIL_STRING_MATCHER_HPP
#define OSMIUM_UTIL_STRING_MATCHER_HPP

/*

This file is part of Osmium (http://osmcode.org/libosmium).

Copyright 2013-2017 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <cstring>
#include <iosfwd>
#include <string>
#include <vector>

// std::regex doesn't work properly in glibc++ before version 4.9 and
// libc++ before version 3.7 (?), so the use is disabled by default.
#ifdef OSMIUM_WITH_REGEX
# include <regex>
#endif

#include <boost/variant.hpp>

namespace osmium {

    /**
     * Implements various string matching functions.
     */
    class StringMatcher {

    public:

        // Parent class for all matcher classes. Used for enable_if check.
        class matcher {
        };

        /**
         * Never matches.
         */
        class always_false : public matcher {

        public:

            bool match(const char* /*test_string*/) const noexcept {
                return false;
            }

            void print(std::ostream& out) const {
                out << "always_false";
            }

        }; // class always_false

        /**
         * Always matches.
         */
        class always_true : public matcher {

        public:

            bool match(const char* /*test_string*/) const noexcept {
                return true;
            }

            void print(std::ostream& out) const {
                out << "always_true";
            }

        }; // class always_true

        /**
         * Matches if the test string is equal to the stored string.
         */
        class equal : public matcher {

            std::string m_str;

        public:

            explicit equal(const std::string& str) :
                m_str(str) {
            }

            explicit equal(const char* str) :
                m_str(str) {
            }

            bool match(const char* test_string) const noexcept {
                return !std::strcmp(m_str.c_str(), test_string);
            }

            void print(std::ostream& out) const {
                out << "equal[" << m_str << ']';
            }

        }; // class equal

        /**
         * Matches if the test string starts with the stored string.
         */
        class prefix : public matcher {

            std::string m_str;

        public:

            explicit prefix(const std::string& str) :
                m_str(str) {
            }

            explicit prefix(const char* str) :
                m_str(str) {
            }

            bool match(const char* test_string) const noexcept {
                return m_str.compare(0, std::string::npos, test_string, 0, m_str.size()) == 0;
            }

            void print(std::ostream& out) const {
                out << "prefix[" << m_str << ']';
            }

        }; // class prefix

        /**
         * Matches if the test string is a substring of the stored string.
         */
        class substring : public matcher {

            std::string m_str;

        public:

            explicit substring(const std::string& str) :
                m_str(str) {
            }

            explicit substring(const char* str) :
                m_str(str) {
            }

            bool match(const char* test_string) const noexcept {
                return std::strstr(test_string, m_str.c_str());
            }

            void print(std::ostream& out) const {
                out << "substring[" << m_str << ']';
            }

        }; // class substring

#ifdef OSMIUM_WITH_REGEX
        /**
         * Matches if the test string matches the regular expression.
         */
        class regex : public matcher {

            std::regex m_regex;

        public:

            explicit regex(const std::regex& regex) :
                m_regex(regex) {
            }

            bool match(const char* test_string) const noexcept {
                return std::regex_search(test_string, m_regex);
            }

            void print(std::ostream& out) const {
                out << "regex";
            }

        }; // class regex
#endif

        /**
         * Matches if the test string is equal to any of the stored strings.
         */
        class list : public matcher {

            std::vector<std::string> m_strings;

        public:

            explicit list() :
                m_strings() {
            }

            explicit list(const std::vector<std::string>& strings) :
                m_strings(strings) {
            }

            list& add_string(const char* str) {
                m_strings.push_back(str);
                return *this;
            }

            list& add_string(const std::string& str) {
                m_strings.push_back(str);
                return *this;
            }

            bool match(const char* test_string) const noexcept {
                for (const auto& s : m_strings) {
                    if (!std::strcmp(s.c_str(), test_string)) {
                        return true;
                    }
                }
                return false;

            }

            void print(std::ostream& out) const {
                out << "list[";
                for (const auto& s : m_strings) {
                    out << '[' << s << ']';
                }
                out << ']';
            }

        }; // class list

    private:

        using matcher_type = boost::variant<always_false,
                                            always_true,
                                            equal,
                                            prefix,
                                            substring,
#ifdef OSMIUM_WITH_REGEX
                                            regex,
#endif
                                            list>;

        matcher_type m_matcher;

        class match_visitor : public boost::static_visitor<bool> {

            const char* m_str;

        public:

            match_visitor(const char* str) noexcept :
                m_str(str) {
            }

            template <typename TMatcher>
            bool operator()(const TMatcher& t) const noexcept {
                return t.match(m_str);
            }

        }; // class match_visitor

        class print_visitor : public boost::static_visitor<void> {

            std::ostream& m_out;

        public:

            print_visitor(std::ostream& out) :
                m_out(out) {
            }

            template <typename TMatcher>
            void operator()(const TMatcher& t) const noexcept {
                t.print(m_out);
            }

        }; // class print_visitor

    public:

        /**
         * Create a string matcher that will never match.
         */
        StringMatcher() :
            m_matcher(always_false{}) {
        }

        /**
         * Create a string matcher that will always or never match based on
         * the argument.
         * Shortcut for
         * @code StringMatcher{StringMatcher::always_true}; @endcode
         * or
         * @code StringMatcher{StringMatcher::always_false}; @endcode
         */
        StringMatcher(bool result) :
            m_matcher(always_false{}) {
            if (result) {
                m_matcher = always_true{};
            }
        }

        /**
         * Create a string matcher that will match the specified string.
         * Shortcut for
         * @code StringMatcher{StringMatcher::equal{str}}; @endcode
         */
        StringMatcher(const char* str) :
            m_matcher(equal{str}) {
        }

        /**
         * Create a string matcher that will match the specified string.
         * Shortcut for
         * @code StringMatcher{StringMatcher::equal{str}}; @endcode
         */
        StringMatcher(const std::string& str) :
            m_matcher(equal{str}) {
        }

#ifdef OSMIUM_WITH_REGEX
        /**
         * Create a string matcher that will match the specified regex.
         * Shortcut for
         * @code StringMatcher{StringMatcher::regex{aregex}}; @endcode
         */
        StringMatcher(const std::regex& aregex) :
            m_matcher(regex{aregex}) {
        }
#endif

        /**
         * Create a string matcher that will match if any of the strings
         * match.
         * Shortcut for
         * @code StringMatcher{StringMatcher::list{strings}}; @endcode
         */
        StringMatcher(const std::vector<std::string>& strings) :
            m_matcher(list{strings}) {
        }

        /**
         * Create a string matcher.
         *
         * @tparam TMatcher Must be one of the matcher classes
         *                  osmium::StringMatcher::always_false, always_true,
         *                  equal, prefix, substring, regex or list.
         */
        template <typename TMatcher, typename std::enable_if<
            std::is_base_of<matcher, TMatcher>::value, int>::type = 0>
        StringMatcher(TMatcher&& matcher) :
            m_matcher(std::forward<TMatcher>(matcher)) {
        }

        /**
         * Match the specified string.
         */
        bool operator()(const char* str) const noexcept {
            return boost::apply_visitor(match_visitor{str}, m_matcher);
        }

        /**
         * Match the specified string.
         */
        bool operator()(const std::string& str) const noexcept {
            return operator()(str.c_str());
        }

        void print(std::ostream& out) const {
            return boost::apply_visitor(print_visitor{out}, m_matcher);
        }

    }; // class StringMatcher

    inline std::ostream& operator<<(std::ostream& out, const StringMatcher& matcher) {
        matcher.print(out);
        return out;
    }

} // namespace osmium

#endif // OSMIUM_UTIL_STRING_MATCHER_HPP
