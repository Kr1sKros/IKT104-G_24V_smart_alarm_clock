/*!
 * @file text_utilities.h
 * @brief A header file with a function capable of retrieving latest news from cnn using their public rss feed.
 * @author Anders L.
 */


#ifndef TEXT_UTILITIES_H
#define TEXT_UTILITIES_H

#include <cstdint>
#include <string>
#include "ThisThread.h"
#include "mbed.h"
#include "DFRobot_RGBLCD1602.h"


class text_utils {
public:
    static std::string extractTitle(const char* xml) {
    const char* startTag = "<title><![CDATA[";
    const char* endTag = "]]></title>";

    // Find the position of the first occurrence of the start tag
    const char* startPos = strstr(xml, startTag);
    if (!startPos) {
        return ""; // Title start tag not found
    }

    // Find the position of the end tag
    const char* endPos = strstr(startPos, endTag);
    if (!endPos) {
        return ""; // Title end tag not found
    }

    // Find the position of the second occurrence of the start tag
    startPos = strstr(endPos, startTag);
    if (!startPos) {
        return ""; // Second title start tag not found
    }
    startPos += strlen(startTag); // Move startPos to the beginning of the title text

    // Find the position of the end tag
    endPos = strstr(startPos, endTag);
    if (!endPos) {
        return ""; // Title end tag not found
    }

    // Extract the title text between startPos and endPos
    return std::string(startPos, endPos - startPos);
    }

    static void printScrolling(DFRobot_RGBLCD1602* lcd, const std::string& text, int delay_ms = 260, uint8_t row = 0) {
        // Calculate the number of iterations needed for scrolling
        int iterations = text.length() - 16;

        // Infinite loop to keep scrolling indefinitely
        while (true) {
            lcd->setCursor(0, row);
            lcd->printf(text.c_str());
            ThisThread::sleep_for(2s);
            // Loop through each possible 16-character segment
            for (int i = 0; i <= iterations; ++i) {
                // Clear the LCD screen
                lcd->setCursor(0, row);
                // Calculate the starting index of the segment
                int start = i % text.length();
                
                // Create a string representing the current segment
                std::string segment = text.substr(start, 16);
                
                // Set the cursor position to the start of the LCD screen
                lcd->setCursor(0, row);
                
                // Print the current segment to the LCD screen
                lcd->printf(segment.c_str());
                
                // Wait for a short while
                ThisThread::sleep_for(std::chrono::milliseconds(delay_ms));
            }
        ThisThread::sleep_for(2s);
        }
    }
};

#endif // TEXT_UTILITIES_H