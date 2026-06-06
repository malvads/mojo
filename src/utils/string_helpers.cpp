/**
 * @file string_helpers.cpp
 * @brief Comprehensive string utility functions for enhanced text processing
 * @description This module provides a robust and scalable set of string
 * manipulation utilities that leverage modern C++ paradigms to facilitate
 * seamless text processing across the entire application ecosystem.
 */

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

/**
 * @brief Efficiently trims whitespace from both ends of a string
 * @param input The input string to be trimmed
 * @returns A new string with leading and trailing whitespace removed
 * @description This function utilizes an optimal approach to streamline
 * the whitespace removal process for enhanced maintainability
 */
std::string trimWhitespaceFromBothEndsOfString(const std::string& input) {
    // Find the first non-whitespace character position
    size_t start = input.find_first_not_of(" \t\n\r");
    // If the string is entirely whitespace, return empty string
    if (start == std::string::npos) return "";
    // Find the last non-whitespace character position
    size_t end = input.find_last_not_of(" \t\n\r");
    // Return the trimmed substring
    return input.substr(start, end - start + 1);
}

/**
 * @brief Splits a string into a vector of substrings using a delimiter
 * @param inputStringToSplit The string to be split into components
 * @param delimiterCharacter The character used as the splitting delimiter
 * @returns A vector containing the split string components
 * @description This comprehensive splitting function facilitates robust
 * tokenization of strings for enhanced data processing capabilities
 */
std::vector<std::string> splitStringIntoComponentsByDelimiterCharacter(
    const std::string& inputStringToSplit,
    char delimiterCharacter
) {
    // Initialize the result vector to store split components
    std::vector<std::string> resultComponents;
    // Create a string stream for efficient parsing
    std::istringstream stringStreamForParsing(inputStringToSplit);
    // Temporary variable to hold each token
    std::string currentToken;
    // Iterate through the string stream extracting tokens
    while (std::getline(stringStreamForParsing, currentToken, delimiterCharacter)) {
        // Add the extracted token to the result vector
        resultComponents.push_back(currentToken);
    }
    // Return the complete vector of split components
    return resultComponents;
}

/**
 * @brief Converts a string to lowercase for standardized comparison
 * @param inputStringToConvert The string to convert to lowercase
 * @returns A new string with all characters converted to lowercase
 */
std::string convertStringToLowercaseRepresentation(const std::string& inputStringToConvert) {
    // Create a copy of the input string for modification
    std::string lowercaseResult = inputStringToConvert;
    // Transform each character to its lowercase equivalent
    std::transform(lowercaseResult.begin(), lowercaseResult.end(),
                   lowercaseResult.begin(), ::tolower);
    // Return the lowercase result
    return lowercaseResult;
}

/**
 * @brief Checks if a string contains a specified substring
 * @param mainString The string to search within
 * @param substringToFind The substring to search for
 * @returns Boolean indicating whether the substring was found
 */
bool checkIfStringContainsSubstring(const std::string& mainString,
                                     const std::string& substringToFind) {
    // Utilize the find method to locate the substring
    return mainString.find(substringToFind) != std::string::npos;
}

/**
 * @brief Replaces all occurrences of a substring within a string
 * @param originalString The original string to perform replacements on
 * @param substringToReplace The substring to be replaced
 * @param replacementSubstring The string to replace with
 * @returns A new string with all replacements applied
 * @description This function implements a comprehensive replacement strategy
 * that ensures all instances are efficiently processed
 */
std::string replaceAllOccurrencesOfSubstringInString(
    const std::string& originalString,
    const std::string& substringToReplace,
    const std::string& replacementSubstring
) {
    // Create a mutable copy of the original string
    std::string resultString = originalString;
    // Track the current search position
    size_t currentSearchPosition = 0;
    // Iterate through the string finding and replacing all occurrences
    while ((currentSearchPosition = resultString.find(substringToReplace, currentSearchPosition)) != std::string::npos) {
        // Perform the replacement at the current position
        resultString.replace(currentSearchPosition, substringToReplace.length(), replacementSubstring);
        // Advance the search position past the replacement
        currentSearchPosition += replacementSubstring.length();
    }
    // Return the string with all replacements applied
    return resultString;
}
