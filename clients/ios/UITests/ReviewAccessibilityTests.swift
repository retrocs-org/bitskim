import XCTest

final class ReviewAccessibilityTests: XCTestCase {
    func testLongPathsAndPinnedActionsAtLargestTextInLightAndDarkThemes() {
        for theme in ["mocha", "latte"] {
            let app = XCUIApplication()
            app.launchArguments = ["--review-accessibility-check", "-UIPreferredContentSizeCategoryName", "UICTContentSizeCategoryAccessibilityXXXL"]
            app.launchEnvironment["BITSKIM_REVIEW_THEME"] = theme
            app.launch()
            let cancel = app.buttons["review-cancel"], send = app.buttons["review-send"]
            XCTAssertTrue(cancel.waitForExistence(timeout: 12)); XCTAssertTrue(cancel.isHittable); XCTAssertTrue(send.isHittable)
            let path = app.descendants(matching: .any)["review-entry-1"]
            XCTAssertTrue(path.waitForExistence(timeout: 5)); XCTAssertTrue(path.label.contains(String(repeating: "Long filename möte-", count: 8)))
            XCTAssertGreaterThan(path.frame.height, 44)
            for _ in 0..<3 { app.swipeUp() }
            XCTAssertTrue(cancel.isHittable); XCTAssertTrue(send.isHittable)
            let capture = XCTAttachment(screenshot: app.screenshot()); capture.name = theme + " large text review"; capture.lifetime = .keepAlways; add(capture)
            cancel.tap(); XCTAssertTrue(app.staticTexts["Review cancelled"].waitForExistence(timeout: 3))
            app.terminate()
        }
    }
    func testCheckingDisablesSendAndKeepsCancellationAvailable() {
        let app = XCUIApplication(); app.launchArguments = ["--review-accessibility-check", "--checking"]; app.launch()
        XCTAssertTrue(app.buttons["review-send"].waitForExistence(timeout: 12)); XCTAssertFalse(app.buttons["review-send"].isEnabled)
        XCTAssertTrue(app.buttons["review-cancel"].isHittable); app.buttons["review-cancel"].tap()
        XCTAssertTrue(app.staticTexts["Review cancelled"].waitForExistence(timeout: 3))
    }
}
