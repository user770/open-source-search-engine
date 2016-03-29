#include "gtest/gtest.h"
#include "Robots.h"
#include "Url.h"
#include "Log.h"

#define TEST_DOMAIN "http://example.com"

//
// test class
//
class TestRobots : public Robots {
public:
	TestRobots( const char* robotsTxt, int32_t robotsTxtLen, const char *userAgent = "testbot" )
		: Robots (robotsTxt, robotsTxtLen, userAgent ) {
	}

	using Robots::getNextLine;
	using Robots::getField;
	using Robots::getValue;

	using Robots::getCurrentLine;
	using Robots::getCurrentLineLen;

	using Robots::isUserAgentFound;
	using Robots::isDefaultUserAgentFound;

	using Robots::isRulesEmpty;
	using Robots::isDefaultRulesEmpty;


	bool isAllowed( const char *path ) {
		char urlStr[1024];
		snprintf( urlStr, 1024, TEST_DOMAIN "%s", path );

		Url url;
		url.set( urlStr );

		return Robots::isAllowed( &url );
	}
};

static void expectRobotsNoNextLine( TestRobots *robots ) {
	EXPECT_FALSE( robots->getNextLine() );
}

static void expectRobots( TestRobots *robots, const char *expectedLine, const char *expectedField = "", const char *expectedValue = "" ) {
	logf(LOG_INFO, "expectLine='%s' expectField='%s' expectValue='%s'", expectedLine, expectedField, expectedValue);
	std::stringstream ss;
	ss << __func__ << ":"
			<< " expectedLine='" << expectedLine << "'"
			<< " expectedField='" << expectedField << "'"
			<< " expectedValue='" << expectedValue << "'";
	SCOPED_TRACE(ss.str());

	EXPECT_TRUE( robots->getNextLine() );
	EXPECT_EQ( strlen( expectedLine ), robots->getCurrentLineLen() );
	EXPECT_EQ( 0, memcmp( expectedLine, robots->getCurrentLine(), robots->getCurrentLineLen() ) );

	if ( expectedField != "" ) {
		const char *field = NULL;
		int32_t fieldLen = 0;

		EXPECT_TRUE( robots->getField( &field, &fieldLen ) );

		EXPECT_EQ( strlen( expectedField ), fieldLen );
		EXPECT_EQ( 0, memcmp( expectedField, field, fieldLen ) );

		if ( expectedValue != "" ) {
			const char *value = NULL;
			int32_t valueLen = 0;

			EXPECT_TRUE( robots->getValue( &value, &valueLen ) );
			EXPECT_EQ( strlen( expectedValue ), valueLen );
			EXPECT_EQ( 0, memcmp( expectedValue, value, valueLen ) );
		}
	}
}

TEST( RobotsTest, RobotsGetNextLineLineEndings ) {
	const char *robotsTxt = "line 1\n"
							"line 2\r"
							"line 3\r\n"
							"line 4\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	expectRobots( &robots, "line 1" );
	expectRobots( &robots, "line 2" );
	expectRobots( &robots, "line 3" );
	expectRobots( &robots, "line 4" );

	expectRobotsNoNextLine( &robots);
}

TEST( RobotsTest, RobotsGetNextLineWhitespaces ) {
	const char *robotsTxt = "   line 1  \n"
							"  line 2    \r"
							"       \n"
							"\tline 3\t\r\n"
							"\t\tline 4   \n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	expectRobots( &robots, "line 1" );
	expectRobots( &robots, "line 2" );
	expectRobots( &robots, "line 3" );
	expectRobots( &robots, "line 4" );

	expectRobotsNoNextLine( &robots);
}

TEST( RobotsTest, RobotsGetNextLineComments ) {
	const char *robotsTxt = "   line 1  # comment \n"
							"  line 2#comment    \r"
							"    # line 2a \n"
							"\tline 3\t#\tcomment\r\n"
							"\t\t#line 4\t\t\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	expectRobots( &robots, "line 1" );
	expectRobots( &robots, "line 2" );
	expectRobots( &robots, "line 3" );

	expectRobotsNoNextLine( &robots);
}

TEST( RobotsTest, RobotsGetFieldValue ) {
	const char *robotsTxt = "   field1: value1  # comment \n"
							"  field2   : value2#comment    \r"
							"    # line 2a \n"
							"\tfield3\t\t:\tvalue3\t#\tcomment\r\n"
							"\t\t#line 4\t\t\n"
							"\tfield4\t\t:\tvalue four#comment\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	expectRobots( &robots, "field1: value1", "field1", "value1" );
	expectRobots( &robots, "field2   : value2", "field2", "value2" );
	expectRobots( &robots, "field3\t\t:\tvalue3", "field3", "value3" );
	expectRobots( &robots, "field4\t\t:\tvalue four", "field4", "value four" );

	expectRobotsNoNextLine( &robots);
}

//
// helper method
//

static void logRobotsTxt( const char *robotsTxt ) {
	logf (LOG_INFO, "===== robots.txt =====\n%s", robotsTxt);
	TestRobots robots( robotsTxt, strlen(robotsTxt) );
}

static void generateRobotsTxt ( char *robotsTxt, size_t robotsTxtSize, int32_t *pos, const char *userAgent = "testbot", const char *allow = "", const char *disallow = "", bool reversed = false ) {
	if ( *pos != 0 ) {
		*pos += snprintf ( robotsTxt + *pos, robotsTxtSize - *pos, "\n" );
	}

	*pos += snprintf ( robotsTxt + *pos, robotsTxtSize - *pos, "user-agent: %s\n", userAgent );

	if ( reversed && disallow != "" ) {
		*pos += snprintf (robotsTxt + *pos, robotsTxtSize - *pos, "disallow: %s\n", disallow );
	}

	if ( allow != "" ) {
		*pos += snprintf ( robotsTxt + *pos, robotsTxtSize - *pos, "allow: %s\n", allow );
	}

	if ( !reversed && disallow != "" ) {
		*pos += snprintf (robotsTxt + *pos, robotsTxtSize - *pos, "disallow: %s\n", disallow );
	}
}

static void generateTestRobotsTxt ( char *robotsTxt, size_t robotsTxtSize, const char *allow = "", const char *disallow = "" ) {
	int32_t pos = 0;
	generateRobotsTxt( robotsTxt, robotsTxtSize, &pos, "testbot", allow, disallow);
	logRobotsTxt( robotsTxt );
}

static void generateTestReversedRobotsTxt ( char *robotsTxt, size_t robotsTxtSize, const char *allow = "", const char *disallow = "" ) {
	int32_t pos = 0;
	generateRobotsTxt( robotsTxt, robotsTxtSize, &pos, "testbot", allow, disallow, true);
	logRobotsTxt( robotsTxt );
}

static bool isUrlAllowed( const char *path, const char *robotsTxt, bool *userAgentFound, bool *hadAllowOrDisallow, const char *userAgent = "testbot" ) {
	char urlStr[1024];
	snprintf( urlStr, 1024, TEST_DOMAIN "%s", path );

	Url url;
	url.set( urlStr );

	int32_t crawlDelay = -1;

	bool isAllowed = Robots::isAllowed( &url, userAgent, robotsTxt, strlen(robotsTxt), userAgentFound, true, &crawlDelay, hadAllowOrDisallow);

	logf( LOG_INFO, "isUrlAllowed: result=%d userAgent='%s' url='%s'", isAllowed, userAgent, urlStr );

	return isAllowed;
}

static bool isUrlAllowed( const char *path, const char *robotsTxt, const char *userAgent = "testbot" ) {
	bool userAgentFound = false;
	bool hadAllowOrDisallow = false;

	bool isAllowed = isUrlAllowed( path, robotsTxt, &userAgentFound, &hadAllowOrDisallow, userAgent );
	EXPECT_TRUE( userAgentFound );
	EXPECT_TRUE( hadAllowOrDisallow );

	return isAllowed;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Test user-agent                                                            //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

TEST( RobotsTest, UserAgentSingleUANoMatch ) {
	char robotsTxt[1024] = "user-agent: abcbot\n"
	                       "crawl-delay: 1\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( -1, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentSingleUAPrefixMatch ) {
	char robotsTxt[1024] = "user-agent: testbotabc\n"
	                       "crawl-delay: 1\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1000, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentSingleUAPrefixVersionMatch ) {
	char robotsTxt[1024] = "user-agent: testbot/1.0\n"
	                       "crawl-delay: 1\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1000, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentSingleUAIgnoreCase ) {
	char robotsTxt[1024] = "user-agent: TestBot/1.0\n"
	                       "crawl-delay: 1\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1000, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentSingleUAMatch ) {
	char robotsTxt[1024] = "user-agent: testbot\n"
	                       "crawl-delay: 1\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1000, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentSeparateUANone ) {
	char robotsTxt[1024] = "user-agent: atestbot\n"
	                       "crawl-delay: 1\n"
	                       "user-agent: abcbot\n"
	                       "crawl-delay: 2\n"
	                       "user-agent: defbot\n"
	                       "crawl-delay: 3\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( -1, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentSeparateUAFirst ) {
	char robotsTxt[1024] = "user-agent: testbot\n"
	                       "crawl-delay: 1\n"
	                       "user-agent: abcbot\n"
	                       "crawl-delay: 2\n"
	                       "user-agent: defbot\n"
	                       "crawl-delay: 3\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1000, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentSeparateUASecond ) {
	char robotsTxt[1024] = "user-agent: abcbot\n"
	                       "crawl-delay: 1\n"
	                       "user-agent: testbot\n"
	                       "crawl-delay: 2\n"
	                       "user-agent: defbot\n"
	                       "crawl-delay: 3\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 2000, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentSeparateUALast ) {
	char robotsTxt[1024] = "user-agent: abcbot\n"
	                       "crawl-delay: 1\n"
	                       "user-agent: defbot\n"
	                       "crawl-delay: 2\n"
	                       "user-agent: testbot\n"
	                       "crawl-delay: 3\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 3000, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentMultiUANone ) {
	char robotsTxt[1024] = "user-agent: abcbot\n"
	                       "crawl-delay: 1\n"
	                       "user-agent: atestbot\n"
	                       "crawl-delay: 2\n"
	                       "user-agent: defbot\n"
	                       "crawl-delay: 3\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( -1, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentMultiUAFirst ) {
	char robotsTxt[1024] = "user-agent: testbot\n"
	                       "user-agent: abcbot\n"
	                       "user-agent: defbot\n"
	                       "crawl-delay: 1\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1000, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentMultiUASecond ) {
	char robotsTxt[1024] = "user-agent: abcbot\n"
	                       "user-agent: testbot\n"
	                       "user-agent: defbot\n"
	                       "crawl-delay: 1\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1000, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentMultiUALast ) {
	char robotsTxt[1024] = "user-agent: abcbot\n"
	                       "user-agent: defbot\n"
	                       "user-agent: testbot\n"
	                       "crawl-delay: 1\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1000, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentDefaultMultiUAFirst ) {
	char robotsTxt[1024] = "user-agent: *\n"
	                       "crawl-delay: 1\n"
	                       "user-agent: testbot\n"
	                       "user-agent: abcbot\n"
	                       "user-agent: defbot\n"
	                       "crawl-delay: 2\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_TRUE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 2000, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentDefaultMultiUASecond ) {
	char robotsTxt[1024] = "user-agent: *\n"
	                       "crawl-delay: 1\n"
	                       "user-agent: abcbot\n"
	                       "user-agent: testbot\n"
	                       "user-agent: defbot\n"
	                       "crawl-delay: 2\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_TRUE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 2000, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentDefaultMultiUALast ) {
	char robotsTxt[1024] = "user-agent: *\n"
	                       "crawl-delay: 1\n"
	                       "user-agent: abcbot\n"
	                       "user-agent: defbot\n"
	                       "user-agent: testbot\n"
	                       "crawl-delay: 2\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_TRUE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 2000, robots.getCrawlDelay() );
}


TEST( RobotsTest, UserAgentMultiDefaultUAFirst ) {
	char robotsTxt[1024] = "user-agent: *\n"
	                       "user-agent: abcbot\n"
	                       "user-agent: defbot\n"
	                       "crawl-delay: 1\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_TRUE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1000, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentMultiDefaultUASecond ) {
	char robotsTxt[1024] = "user-agent: abcbot\n"
	                       "user-agent: *\n"
	                       "user-agent: defbot\n"
	                       "crawl-delay: 1\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_TRUE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1000, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentMultiDefaultUALast ) {
	char robotsTxt[1024] = "user-agent: abcbot\n"
	                       "user-agent: defbot\n"
	                       "user-agent: *\n"
	                       "crawl-delay: 1\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_TRUE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1000, robots.getCrawlDelay() );
}

TEST( RobotsTest, UserAgentFieldCaseInsensitive ) {
	char robotsTxt[1024] = "User-Agent: testbot\n"
	                       "crawl-delay: 1\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1000, robots.getCrawlDelay() );
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Test comments                                                              //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

TEST(RobotsTest, DISABLED_CommentsFullLine) {
/// @todo ALC
}

TEST(RobotsTest, DISABLED_CommentsAfterWithSpace) {
	int32_t pos = 0;
	char robotsTxt[1024];
	generateRobotsTxt( robotsTxt, 1024, &pos, "testbot #user-agent", "/test #allow", "/ #disallow");
	logRobotsTxt( robotsTxt );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( isUrlAllowed( "/", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/index.html", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/test.html", robotsTxt ) );
}

TEST(RobotsTest, DISABLED_CommentsAfterNoSpace) {
	int32_t pos = 0;
	char robotsTxt[1024];
	generateRobotsTxt( robotsTxt, 1024, &pos, "testbot#user-agent", "/test#allow", "/#disallow");
	logRobotsTxt( robotsTxt );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( isUrlAllowed( "/", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/index.html", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/test.html", robotsTxt ) );
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Test whitespace                                                            //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

TEST( RobotsTest, WhitespaceSpaceDirectiveBefore ) {
	int32_t pos = 0;
	char robotsTxt[1024];
	pos += snprintf( robotsTxt + pos, 1024 - pos, "    user-agent:%s\n", "testbot" );
	pos += snprintf( robotsTxt + pos, 1024 - pos, "        disallow:%s\n", "/test" );
	logRobotsTxt( robotsTxt );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( isUrlAllowed( "/", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/index.html", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/test.html", robotsTxt ) );
}

TEST( RobotsTest, WhitespaceSpaceDirectiveAfter ) {
	int32_t pos = 0;
	char robotsTxt[1024];
	pos += snprintf( robotsTxt + pos, 1024 - pos, "user-agent:   %s\n", "testbot" );
	pos += snprintf( robotsTxt + pos, 1024 - pos, "disallow:     %s\n", "/test" );
	logRobotsTxt( robotsTxt );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( isUrlAllowed( "/", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/index.html", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/test.html", robotsTxt ) );
}

TEST( RobotsTest, WhitespaceSpaceDirectiveBoth ) {
	int32_t pos = 0;
	char robotsTxt[1024];
	pos += snprintf( robotsTxt + pos, 1024 - pos, "    user-agent:    %s\n", "testbot" );
	pos += snprintf( robotsTxt + pos, 1024 - pos, "        disallow:  %s\n", "/test" );
	logRobotsTxt( robotsTxt );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( isUrlAllowed( "/", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/index.html", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/test.html", robotsTxt ) );
}

TEST( RobotsTest, WhitespaceTabsDirectiveBefore ) {
	int32_t pos = 0;
	char robotsTxt[1024];
	pos += snprintf( robotsTxt + pos, 1024 - pos, "\tuser-agent:%s\n", "testbot" );
	pos += snprintf( robotsTxt + pos, 1024 - pos, "\t\tdisallow:%s\n", "/test" );
	logRobotsTxt( robotsTxt );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( isUrlAllowed( "/", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/index.html", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/test.html", robotsTxt ) );
}

TEST( RobotsTest, WhitespaceTabsDirectiveAfter ) {
	int32_t pos = 0;
	char robotsTxt[1024];
	pos += snprintf( robotsTxt + pos, 1024 - pos, "user-agent:\t%s\n", "testbot" );
	pos += snprintf( robotsTxt + pos, 1024 - pos, "disallow:\t%s\n", "/test" );
	logRobotsTxt( robotsTxt );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( isUrlAllowed( "/", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/index.html", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/test.html", robotsTxt ) );
}

TEST( RobotsTest, WhitespaceTabsDirectiveBoth ) {
	int32_t pos = 0;
	char robotsTxt[1024];
	pos += snprintf( robotsTxt + pos, 1024 - pos, "\tuser-agent:\t%s\n", "testbot" );
	pos += snprintf( robotsTxt + pos, 1024 - pos, "\t\tdisallow:\t%s\n", "/test" );
	logRobotsTxt( robotsTxt );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( isUrlAllowed( "/", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/index.html", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/test.html", robotsTxt ) );
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Test allow/disallow                                                        //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

TEST( RobotsTest, AllowAll ) {
	static const char *allow = "";
	static const char *disallow = " ";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( isUrlAllowed( "/", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/index.html", robotsTxt ) );
}

TEST( RobotsTest, DisallowAll ) {
	static const char *allow = "";
	static const char *disallow = "/";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( robots.isAllowed( "/" ) );
	EXPECT_FALSE( robots.isAllowed( "/index.html" ) );
}

// /123 matches /123 and /123/ and /1234 and /123/456
TEST( RobotsTest, PathMatch ) {
	static const char *allow = "";
	static const char *disallow = "/123";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isAllowed( "/" ) );
	EXPECT_TRUE( robots.isAllowed( "/index.html" ) );
	EXPECT_TRUE( robots.isAllowed( "/12" ) );

	EXPECT_FALSE( robots.isAllowed( "/123" ) );
	EXPECT_FALSE( robots.isAllowed( "/123/" ) );
	EXPECT_FALSE( robots.isAllowed( "/1234" ) );
	EXPECT_FALSE( robots.isAllowed( "/123/456" ) );
}

// /123/ matches /123/ and /123/456
TEST( RobotsTest, PathMatchWithEndSlash ) {
	static const char *allow = "";
	static const char *disallow = "/123/";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isAllowed( "/" ) );
	EXPECT_TRUE( robots.isAllowed( "/index.html" ) );
	EXPECT_TRUE( robots.isAllowed( "/123" ) );
	EXPECT_TRUE( robots.isAllowed( "/1234" ) );

	EXPECT_FALSE( robots.isAllowed( "/123/" ) );
	EXPECT_FALSE( robots.isAllowed( "/123/456" ) );
	EXPECT_FALSE( robots.isAllowed( "/123/456/" ) );
}

// /*abc matches /123abc and /123/abc and /123abc456 and /123/abc/456
TEST( RobotsTest, DISABLED_PathMatchWildcardStart ) {
	static const char *allow = "";
	static const char *disallow = "/*abc";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( isUrlAllowed( "/123", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/123ab", robotsTxt ) );

	EXPECT_FALSE( isUrlAllowed( "/123abc", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/123/abc", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/123abc456", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/123/abc/456", robotsTxt ) );
}

// /123*xyz matches /123qwertyxyz and /123/qwerty/xyz/789
TEST( RobotsTest, DISABLED_PathMatchWildcardMid ) {
	static const char *allow = "";
	static const char *disallow = "/123*xyz";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( isUrlAllowed( "/123/qwerty/xy", robotsTxt ) );

	EXPECT_FALSE( isUrlAllowed( "/123qwertyxyz", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/123qwertyxyz/", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/123/qwerty/xyz/789", robotsTxt ) );
}

// /123$ matches ONLY /123
TEST( RobotsTest, PathMatchEnd ) {
	static const char *allow = "";
	static const char *disallow = "/123$";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isAllowed( "/123/" ) );

	EXPECT_FALSE( robots.isAllowed( "/123" ) );
}

// /*abc$ matches /123abc and /123/abc but NOT /123/abc/x etc.
TEST( RobotsTest, DISABLED_PathMatchWildcardEnd ) {
	static const char *allow = "";
	static const char *disallow = "/*abc$";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( isUrlAllowed( "/123/abc/x", robotsTxt ) );

	EXPECT_FALSE( isUrlAllowed( "/123abc", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/123/abc", robotsTxt ) );
}

/// @todo ALC test multiple wildcard

/// @todo ALC test multiple wildcard end

/// @todo ALC test _escaped_fragment_

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Test crawl delay                                                           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

TEST( RobotsTest, CrawlDelayValueNone ) {
	char robotsTxt[1024] = "user-agent: testbot\n"
	                       "crawl-delay:";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( -1, robots.getCrawlDelay() );
}

TEST( RobotsTest, CrawlDelayValueInvalid ) {
	char robotsTxt[1024] = "user-agent: testbot\n"
	                       "crawl-delay: abc";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( -1, robots.getCrawlDelay() );
}

TEST( RobotsTest, CrawlDelayNoMatch ) {
	char robotsTxt[1024] = "user-agent: abcbot\n"
	                       "crawl-delay: 1";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( -1, robots.getCrawlDelay() );
}

TEST( RobotsTest, CrawlDelayMissing ) {
	char robotsTxt[1024] = "user-agent: testbot\n"
	                       "disallow: /";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_FALSE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( -1, robots.getCrawlDelay() );
}

TEST( RobotsTest, CrawlDelayValueFractionPartial ) {
	char robotsTxt[1024] = "user-agent: testbot\n"
	                       "crawl-delay: .5";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 500, robots.getCrawlDelay() );
}

TEST( RobotsTest, CrawlDelayValueFractionFull ) {
	char robotsTxt[1024] = "user-agent: testbot\n"
	                       "crawl-delay: 1.5\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1500, robots.getCrawlDelay() );
}

TEST( RobotsTest, CrawlDelayValueIntegerValid ) {
	char robotsTxt[1024] = "user-agent: testbot\n"
	                       "crawl-delay: 30 \n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 30000, robots.getCrawlDelay() );
}

TEST( RobotsTest, CrawlDelayValueIntegerInvalid ) {
	char robotsTxt[1024] = "user-agent: testbot\n"
	                       "crawl-delay: 60abc \n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( -1, robots.getCrawlDelay() );
}

TEST( RobotsTest, CrawlDelayValueComment ) {
	char robotsTxt[1024] = "user-agent: testbot\n"
	                       "crawl-delay: 60#abc \n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 60000, robots.getCrawlDelay() );
}

TEST( RobotsTest, CrawlDelayDefaultFirstNoMatch ) {
	char robotsTxt[1024] = "user-agent: *\n"
	                       "crawl-delay: 1 \n"
	                       "user-agent: testbot\n"
	                       "crawl-delay: 2 \n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_TRUE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 2000, robots.getCrawlDelay() );
}

TEST( RobotsTest, CrawlDelayDefaultLastNoMatch ) {
	char robotsTxt[1024] = "user-agent: testbot\n"
	                       "crawl-delay: 1 \n"
	                       "user-agent: * \n"
	                       "crawl-delay: 2 \n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_TRUE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1000, robots.getCrawlDelay() );
}

TEST( RobotsTest, CrawlDelayDefaultFirstMatch ) {
	char robotsTxt[1024] = "user-agent: *\n"
	                       "crawl-delay: 1 \n"
	                       "user-agent: abcbot\n"
	                       "crawl-delay: 2 \n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_TRUE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1000, robots.getCrawlDelay() );
}

TEST( RobotsTest, CrawlDelayDefaultLastMatch ) {
	char robotsTxt[1024] = "user-agent: abcbot\n"
	                       "crawl-delay: 1 \n"
	                       "user-agent: *\n"
	                       "crawl-delay: 2 \n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_TRUE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 2000, robots.getCrawlDelay() );
}

TEST( RobotsTest, CrawlDelayFieldCaseInsensitive ) {
	char robotsTxt[1024] = "user-agent: testbot\n"
	                       "Crawl-Delay: 1\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isUserAgentFound() );
	EXPECT_TRUE( robots.isRulesEmpty() );
	EXPECT_FALSE( robots.isDefaultUserAgentFound() );
	EXPECT_TRUE( robots.isDefaultRulesEmpty() );
	EXPECT_EQ( 1000, robots.getCrawlDelay() );
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Test site map                                                              //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

/// @todo ALC

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Test line endings                                                          //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

/// @todo ALC

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Test utf-8 encoding (non-ascii)                                            //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

/// @todo ALC

//////////////////////////////////////////////////////////////////////////////////////////
//                                                                                      //
// Test cases based on google's robots.txt specification                                //
// https://developers.google.com/webmasters/control-crawl-index/docs/robots_txt         //
//     #example-path-matches                                                            //
//                                                                                      //
//////////////////////////////////////////////////////////////////////////////////////////

// [path]		[match]								[no match]					[comments]
// /			any valid url													Matches the root and any lower level URL
// /*			equivalent to /						equivalent to /				Equivalent to "/" -- the trailing wildcard is ignored.
TEST( RobotsTest, GPathMatchDisallowAll ) {
	static const char *allow = "";
	static const char *disallow = "/";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( isUrlAllowed( "/", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/index.html", robotsTxt ) );
}

TEST( RobotsTest, DISABLED_GPathMatchDisallowAllWildcard ) {
	static const char *allow = "";
	static const char *disallow = "/*";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( isUrlAllowed( "/", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/index.html", robotsTxt ) );
}

// [path]		[match]								[no match]					[comments]
// /fish		/fish								/Fish.asp					Note the case-sensitive matching.
// 				/fish.html							/catfish
// 				/fish/salmon.html					/?id=fish
// 				/fishheads
// 				/fishheads/yummy.html
// 				/fish.php?id=anything
TEST( RobotsTest, GPathMatchPrefixDisallow ) {
	static const char *allow = "";
	static const char *disallow = "/fish";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( robots.isAllowed( "/fish" ) );
	EXPECT_FALSE( robots.isAllowed( "/fish.html" ) );
	EXPECT_FALSE( robots.isAllowed( "/fish/salmon.html" ) );
	EXPECT_FALSE( robots.isAllowed( "/fishheads" ) );
	EXPECT_FALSE( robots.isAllowed( "/fishheads/yummy.html" ) );
	EXPECT_FALSE( robots.isAllowed( "/fish.php?id=anything" ) );

	EXPECT_TRUE( robots.isAllowed( "/Fish.asp" ) );
	EXPECT_TRUE( robots.isAllowed( "/catfish" ) );
	EXPECT_TRUE( robots.isAllowed( "/?id=fish" ) );
}

TEST( RobotsTest, GPathMatchPrefixAllow ) {
	static const char *allow = "/fish";
	static const char *disallow = "/";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isAllowed( "/fish" ) );
	EXPECT_TRUE( robots.isAllowed( "/fish.html" ) );
	EXPECT_TRUE( robots.isAllowed( "/fish/salmon.html" ) );
	EXPECT_TRUE( robots.isAllowed( "/fishheads" ) );
	EXPECT_TRUE( robots.isAllowed( "/fishheads/yummy.html" ) );
	EXPECT_TRUE( robots.isAllowed( "/fish.php?id=anything" ) );

	EXPECT_FALSE( robots.isAllowed( "/Fish.asp" ) );
	EXPECT_FALSE( robots.isAllowed( "/catfish" ) );
	EXPECT_FALSE( robots.isAllowed( "/?id=fish" ) );
}

// [path]		[match]								[no match]					[comments]
// /fish*		/fish								/Fish.asp					Equivalent to "/fish" -- the trailing wildcard is ignored.
// 				/fish.html							/catfish
// 				/fish/salmon.html					/?id=fish
// 				/fishheads
// 				/fishheads/yummy.html
// 				/fish.php?id=anything
TEST( RobotsTest, DISABLED_GPathMatchPrefixWildcardDisallow ) {
	static const char *allow = "";
	static const char *disallow = "/fish*";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( isUrlAllowed( "/fish", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/fish.html", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/fish/salmon.html", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/fishheads", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/fishheads/yummy.html", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/fish.php?id=anything", robotsTxt ) );

	EXPECT_TRUE( isUrlAllowed( "/Fish.asp", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/catfish", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/?id=fish", robotsTxt ) );
}

TEST( RobotsTest, DISABLED_GPathMatchPrefixWildcardAllow ) {
	static const char *allow = "/fish*";
	static const char *disallow = "/";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( isUrlAllowed( "/fish", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/fish.html", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/fish/salmon.html", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/fishheads", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/fishheads/yummy.html", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/fish.php?id=anything", robotsTxt ) );

	EXPECT_FALSE( isUrlAllowed( "/Fish.asp", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/catfish", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/?id=fish", robotsTxt ) );
}

// [path]		[match]								[no match]					[comments]
// /fish/		/fish/								/fish						The trailing slash means this matches anything in this folder.
// 				/fish/?id=anything					/fish.html
// 				/fish/salmon.htm					/Fish/Salmon.php
TEST( RobotsTest, GPathMatchPrefixDirDisallow ) {
	static const char *allow = "";
	static const char *disallow = "/fish/";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( robots.isAllowed( "/fish/" ) );
	EXPECT_FALSE( robots.isAllowed( "/fish/?id=anything" ) );
	EXPECT_FALSE( robots.isAllowed( "/fish/salmon.htm" ) );

	EXPECT_TRUE( robots.isAllowed( "/fish" ) );
	EXPECT_TRUE( robots.isAllowed( "/fish.html" ) );
	EXPECT_TRUE( robots.isAllowed( "/Fish/Salmon.php" ) );
}

TEST( RobotsTest, GPathMatchPrefixDirAllow ) {
	static const char *allow = "/fish/";
	static const char *disallow = "/";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( robots.isAllowed( "/fish/" ) );
	EXPECT_TRUE( robots.isAllowed( "/fish/?id=anything" ) );
	EXPECT_TRUE( robots.isAllowed( "/fish/salmon.htm" ) );

	EXPECT_FALSE( robots.isAllowed( "/fish" ) );
	EXPECT_FALSE( robots.isAllowed( "/fish.html" ) );
	EXPECT_FALSE( robots.isAllowed( "/Fish/Salmon.php" ) );
}

// [path]		[match]								[no match]					[comments]
// *.php		/filename.php						/ 							(even if it maps to /index.php)
// 				/folder/filename.php				/windows.PHP
// 				/folder/filename.php?parameters
// 				/folder/any.php.file.html
// 				/filename.php/
TEST( RobotsTest, DISABLED_GPathMatchWildcardExtDisallow ) {
	static const char *allow = "";
	static const char *disallow = "*.php";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( isUrlAllowed( "/filename.php", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/folter/filename.php", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/folder/filename.php?parameters", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/folder/any.php.file.html", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/filename.php/", robotsTxt ) );

	EXPECT_TRUE( isUrlAllowed( "/", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/windows.PHP", robotsTxt ) );
}

TEST( RobotsTest, DISABLED_GPathMatchWildcardExtAllow ) {
	static const char *allow = "/*.php";
	static const char *disallow = "/";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( isUrlAllowed( "/filename.php", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/folter/filename.php", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/folder/filename.php?parameters", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/folder/any.php.file.html", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/filename.php/", robotsTxt ) );

	EXPECT_FALSE( isUrlAllowed( "/", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/windows.PHP", robotsTxt ) );
}

// [path]		[match]								[no match]					[comments]
// /*.php$		/filename.php						/filename.php?parameters
// 				/folder/filename.php				/filename.php/
// 													/filename.php5
// 													/windows.PHP
TEST( RobotsTest, DISABLED_GPathMatchWildcardExtEndDisallow ) {
	static const char *allow = "";
	static const char *disallow = "/*.php$";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( isUrlAllowed( "/filename.php", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/folder/filename.php", robotsTxt ) );

	EXPECT_TRUE( isUrlAllowed( "/filename.php?parameters", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/filename.php/", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/filename.php5", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/windows.PHP", robotsTxt ) );
}

TEST( RobotsTest, DISABLED_GPathMatchWildcardExtEndAllow ) {
	static const char *allow = "/*.php$";
	static const char *disallow = "/";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( isUrlAllowed( "/filename.php", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/folder/filename.php", robotsTxt ) );

	EXPECT_FALSE( isUrlAllowed( "/filename.php?parameters", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/filename.php/", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/filename.php5", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/windows.PHP", robotsTxt ) );
}

// [path]		[match]								[no match]					[comments]
// /fish*.php	/fish.php							/Fish.PHP
// 				/fishheads/catfish.php?parameters
TEST( RobotsTest, DISABLED_GPathMatchPrefixWildcardExtDisallow ) {
	static const char *allow = "";
	static const char *disallow = "/fish*.php";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_FALSE( isUrlAllowed( "/fish.php", robotsTxt ) );
	EXPECT_FALSE( isUrlAllowed( "/fishheads/catfish.php?parameters", robotsTxt ) );

	EXPECT_TRUE( isUrlAllowed( "/Fish.PHP", robotsTxt ) );
}

TEST( RobotsTest, DISABLED_GPathMatchPrefixWildcardExtAllow ) {
	static const char *allow = "/fish*.php";
	static const char *disallow = "/";

	char robotsTxt[1024];
	generateTestRobotsTxt( robotsTxt, 1024, allow, disallow );

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	EXPECT_TRUE( isUrlAllowed( "/fish.php", robotsTxt ) );
	EXPECT_TRUE( isUrlAllowed( "/fishheads/catfish.php?parameters", robotsTxt ) );

	EXPECT_FALSE( isUrlAllowed( "/Fish.PHP", robotsTxt ) );
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                                                                      //
// Test cases based on google's robots.txt specification                                //
// https://developers.google.com/webmasters/control-crawl-index/docs/robots_txt         //
//     #order-of-precedence-for-group-member-records                                    //
//                                                                                      //
//////////////////////////////////////////////////////////////////////////////////////////

// [url]							[allow]		[disallow]		[verdict]
// http://example.com/page			/p			/				allow
// http://example.com/folder/page	/folder/	/folder			allow
// http://example.com/page.htm		/page		/*.htm			undefined
// http://example.com/				/$			/				allow
// http://example.com/page.htm		/$			/				disallow
TEST( RobotsTest, GPrecedenceAllowDisallow ) {
	char robotsTxt[1024];

	{
		generateTestRobotsTxt( robotsTxt, 1024, "/p", "/" );

		TestRobots robots( robotsTxt, strlen(robotsTxt) );

		EXPECT_TRUE( robots.isAllowed( "/page" ));
	}

	{
		generateTestRobotsTxt( robotsTxt, 1024, "/folder/", "/folder" );

		TestRobots robots( robotsTxt, strlen(robotsTxt) );

		EXPECT_TRUE( robots.isAllowed( "/folder/page" ));
	}

	{
		/// @todo ALC decide what's the result
		generateTestRobotsTxt( robotsTxt, 1024, "/page.h", "/*.htm" );

		TestRobots robots( robotsTxt, strlen(robotsTxt) );

		// EXPECT_TRUE( robots.isAllowed ( "/page.htm" ) );
	}

	{
		generateTestRobotsTxt( robotsTxt, 1024, "/$", "/" );

		TestRobots robots( robotsTxt, strlen(robotsTxt) );

		EXPECT_TRUE( robots.isAllowed( "/" ));
		EXPECT_FALSE( robots.isAllowed( "/page.htm" ));
	}
}

TEST( RobotsTest, GPrecedenceDisallowAllow ) {
	char robotsTxt[1024];

	{
		generateTestReversedRobotsTxt( robotsTxt, 1024, "/p", "/" );

		TestRobots robots( robotsTxt, strlen(robotsTxt) );

		EXPECT_TRUE( robots.isAllowed( "/page" ));
	}

	{
		generateTestReversedRobotsTxt( robotsTxt, 1024, "/folder/", "/folder" );

		TestRobots robots( robotsTxt, strlen( robotsTxt ));

		EXPECT_TRUE( robots.isAllowed( "/folder/page" ));
	}

	{
		/// @todo ALC decide what's the result
		generateTestReversedRobotsTxt( robotsTxt, 1024, "/page.h", "/*.htm" );

		TestRobots robots( robotsTxt, strlen( robotsTxt ));

		// EXPECT_TRUE( robots.isAllowed ( "/page.htm" ) );
	}

	{
		generateTestReversedRobotsTxt( robotsTxt, 1024, "/$", "/" );

		TestRobots robots( robotsTxt, strlen( robotsTxt ));

		EXPECT_TRUE( robots.isAllowed( "/" ));
		EXPECT_FALSE( robots.isAllowed( "/page.htm" ));
	}
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Test cases based on RFC                                                    //
// http://www.robotstxt.org/norobots-rfc.txt                                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

/// @todo ALC

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Test real robots.txt                                                       //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

/// @todo ALC

TEST( RobotsTest, RRobotsEmpty ) {
	char robotsTxt[1024] = { 0 };

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	bool userAgentFound = false;
	bool hadAllowOrDisallow = false;

	EXPECT_TRUE( isUrlAllowed( "/", robotsTxt, &userAgentFound, &hadAllowOrDisallow ) );
	EXPECT_FALSE( userAgentFound );
	EXPECT_FALSE( hadAllowOrDisallow );

	EXPECT_TRUE( isUrlAllowed( "/index.html", robotsTxt, &userAgentFound, &hadAllowOrDisallow ) );
	EXPECT_FALSE( userAgentFound );
	EXPECT_FALSE( hadAllowOrDisallow );
}

TEST( RobotsTest, RRobotsCommentsOnly ) {
	char robotsTxt[1024] =
			"# See http://www.robotstxt.org/robotstxt.html for documentation on how to use the robots.txt file\n"
			"#\n"
			"# To ban all spiders from the entire site uncomment the next two lines:\n"
			"# User-agent: *\n"
			"# Disallow: /\n";

	TestRobots robots( robotsTxt, strlen(robotsTxt) );

	bool userAgentFound = false;
	bool hadAllowOrDisallow = false;

	EXPECT_TRUE( isUrlAllowed( "/", robotsTxt, &userAgentFound, &hadAllowOrDisallow ) );
	EXPECT_FALSE( userAgentFound );
	EXPECT_FALSE( hadAllowOrDisallow );

	EXPECT_TRUE( isUrlAllowed( "/index.html", robotsTxt, &userAgentFound, &hadAllowOrDisallow ) );
	EXPECT_FALSE( userAgentFound );
	EXPECT_FALSE( hadAllowOrDisallow );
}

