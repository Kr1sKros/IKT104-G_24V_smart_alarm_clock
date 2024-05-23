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

#include <iostream>

class text_utils {
public:
    // method used to extract the news headers from the xml format used by the rss feed
    static std::string extractTitle(const char* xml) {
        const char* startTag = "<title><![CDATA[";
        const char* endTag = "]]></title>";
        const int maxDescriptions = 3;
        
        std::string result;
        int count = 0;

        const char* startPos = xml;
        const char* endPos = nullptr;

        // Skip the first description
        startPos = strstr(startPos, startTag);
        if (startPos) {
            startPos += strlen(startTag); // Move startPos to the beginning of the description text
            endPos = strstr(startPos, endTag);
            if (endPos) {
                // Move startPos past the first end tag
                startPos = endPos + strlen(endTag);
            }
        }

        while (count < maxDescriptions) {
            // Find the position of the start tag
            startPos = strstr(startPos, startTag);
            if (!startPos) {
                cout << "end of buffer \n";
                break; // No more descriptions found
            }
            startPos += strlen(startTag); // Move startPos to the beginning of the description text

            // Find the position of the end tag
            endPos = strstr(startPos, endTag);
            if (!endPos) {
                break; // End tag not found
            }

            // Extract the description text between startPos and endPos
            if (!result.empty()) {
                result += " || "; // Add a couple of spaces between descriptions
            }
            result += std::string(startPos, endPos - startPos);

            // Move startPos past the current end tag for the next iteration
            startPos = endPos + strlen(endTag);

            count++;
        }

        std::cout << result << std::endl;
        return result;
    }

    // Method that makes the news headers scroll on the lcd display
    static void printScrolling(DFRobot_RGBLCD1602* lcd, const std::string& text, bool display_prefix = false, int delay_ms = 260, uint8_t row = 0) {
        // Calculate the number of iterations needed for scrolling
        int iterations = text.length() - 16;

        for (int i = 0; i <= 2; i++) {
           // Display the prefix if required
            if (display_prefix) {
                lcd->setCursor(0, row);
                lcd->printf("Top News CNN:");
            }
            ThisThread::sleep_for(2s);

            lcd->setCursor(0, row);
            // Calculate the starting index of the segment
            int start = 0;

            // Create a string representing the current segment
            std::string segment = text.substr(start, 16);
            // Set the cursor position to the start of the LCD screen
            lcd->setCursor(0, row);

            // Print the current segment to the LCD screen
            lcd->printf(segment.c_str());
            ThisThread::sleep_for(2s);

            // Loop through each possible 16-character segment
            for (int i = 0; i <= iterations; ++i) {
                // Clear the LCD screen
                lcd->setCursor(0, row);
                // Calculate the starting index of the segment
                start = i % text.length();

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