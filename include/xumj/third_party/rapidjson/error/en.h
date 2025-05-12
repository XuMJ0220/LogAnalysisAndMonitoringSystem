#pragma once

#include "../document.h"

namespace rapidjson {

// 获取解析错误的英文描述
inline const char* GetParseError_En(ParseErrorCode code) {
    switch (code) {
        case kParseErrorNone:
            return "No error.";
        case kParseErrorDocumentEmpty:
            return "The document is empty.";
        case kParseErrorDocumentRootNotSingular:
            return "The document root must not be followed by other values.";
        case kParseErrorValueInvalid:
            return "Invalid value.";
        case kParseErrorObjectMissName:
            return "Missing a name for object member.";
        case kParseErrorObjectMissColon:
            return "Missing a colon after a name of object member.";
        case kParseErrorObjectMissCommaOrCurlyBracket:
            return "Missing a comma or '}' after an object member.";
        case kParseErrorArrayMissCommaOrSquareBracket:
            return "Missing a comma or ']' after an array element.";
        case kParseErrorStringUnicodeEscapeInvalidHex:
            return "Incorrect hex digit after \\u escape in string.";
        case kParseErrorStringUnicodeSurrogateInvalid:
            return "The surrogate pair in string is invalid.";
        case kParseErrorStringEscapeInvalid:
            return "Invalid escape character in string.";
        case kParseErrorStringMissQuotationMark:
            return "Missing a closing quotation mark in string.";
        case kParseErrorStringInvalidEncoding:
            return "Invalid encoding in string.";
        case kParseErrorNumberTooBig:
            return "Number too big to be stored in double.";
        case kParseErrorNumberMissFraction:
            return "Miss fraction part in number.";
        case kParseErrorNumberMissExponent:
            return "Miss exponent in number.";
        case kParseErrorTermination:
            return "Parsing was terminated.";
        case kParseErrorUnspecificSyntaxError:
            return "Unspecific syntax error.";
        default:
            return "Unknown error.";
    }
}

} // namespace rapidjson 