/*
 * string.h - string manipulation functions for fusion os
 * 
 * implements essential string functions without standard library
 */

#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

/* basic string functions */
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t n);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);
char* strdup(const char* str);

/* memory functions */
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void* ptr1, const void* ptr2, size_t n);
void* memset(void* ptr, int value, size_t n);

/* character functions */
int isdigit(int c);
int isalpha(int c);
int isalnum(int c);
int isspace(int c);
int toupper(int c);
int tolower(int c);

/* string parsing */
char* strtok(char* str, const char* delim);
char* strstr(const char* haystack, const char* needle);
unsigned long strtoul(const char* str, char** endptr, int base);
long strtol(const char* str, char** endptr, int base);

/* formatted output */
int sprintf(char* str, const char* format, ...);
int snprintf(char* str, size_t size, const char* format, ...);
int vsprintf(char* str, const char* format, va_list args);
int vsnprintf(char* str, size_t size, const char* format, va_list args);

/* utility functions */
void reverse(char* str, int len);
int int_to_str(int num, char* str, int base);
int double_to_str(double num, char* str, int precision);

/* memory allocation (simplified) */
void* malloc(size_t size);
void free(void* ptr);

#endif /* STRING_H */