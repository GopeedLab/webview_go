package main

/*
// The fixture uses cleartext HTTP on a synthetic domain. Keep this test-only
// ATS exception in the executable rather than changing library networking policy.
__attribute__((used, section("__TEXT,__info_plist"))) static const char profile_check_plist[] =
"<?xml version=\"1.0\"?><plist version=\"1.0\"><dict>"
"<key>CFBundleIdentifier</key><string>com.gopeed.webview.profilecheck</string>"
"<key>NSAppTransportSecurity</key><dict><key>NSAllowsArbitraryLoads</key><true/></dict>"
"</dict></plist>";
*/
import "C"
