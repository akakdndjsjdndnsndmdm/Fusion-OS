/*
 * string.c - string manipulation functions implementation
 * 
 * implements essential string functions without standard library
 */

#include "string.h"

/* calculate string length */
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

/* copy string */
char* strcpy(char* dest, const char* src) {
    char* original_dest = dest;
    while ((*dest++ = *src++) != '\0') {
        /* copy until null terminator */
    }
    return original_dest;
}

/* copy string with limit */
char* strncpy(char* dest, const char* src, size_t n) {
    char* original_dest = dest;
    while (n > 0 && *src != '\0') {
        *dest++ = *src++;
        n--;
    }
    while (n > 0) {
        *dest++ = '\0';
        n--;
    }
    return original_dest;
}

/* compare strings */
int strcmp(const char* str1, const char* str2) {
    while (*str1 != '\0' && *str1 == *str2) {
        str1++;
        str2++;
    }
    return (*str1 - *str2);
}

/* compare strings with limit */
int strncmp(const char* str1, const char* str2, size_t n) {
    while (n > 0 && *str1 != '\0' && *str1 == *str2) {
        str1++;
        str2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return (*str1 - *str2);
}

/* concatenate strings */
char* strcat(char* dest, const char* src) {
    char* original_dest = dest;
    while (*dest != '\0') {
        dest++;
    }
    while ((*dest++ = *src++) != '\0') {
        /* copy src to dest */
    }
    return original_dest;
}

/* concatenate strings with limit */
char* strncat(char* dest, const char* src, size_t n) {
    char* original_dest = dest;
    while (*dest != '\0') {
        dest++;
    }
    while (n > 0 && *src != '\0') {
        *dest++ = *src++;
        n--;
    }
    *dest = '\0';
    return original_dest;
}

/* duplicate string */
char* strdup(const char* str) {
    size_t len = strlen(str);
    char* new_str = (char*)malloc(len + 1);
    if (new_str != NULL) {
        strcpy(new_str, str);
    }
    return new_str;
}

/* copy memory */
void* memcpy(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (n > 0) {
        *d++ = *s++;
        n--;
    }
    return dest;
}

/* move memory */
void* memmove(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    if (d < s) {
        /* copy forward */
        while (n > 0) {
            *d++ = *s++;
            n--;
        }
    } else {
        /* copy backward */
        d += n;
        s += n;
        while (n > 0) {
            *--d = *--s;
            n--;
        }
    }
    return dest;
}

/* compare memory */
int memcmp(const void* ptr1, const void* ptr2, size_t n) {
    const char* p1 = (const char*)ptr1;
    const char* p2 = (const char*)ptr2;
    while (n > 0) {
        if (*p1 != *p2) {
            return (*p1 - *p2);
        }
        p1++;
        p2++;
        n--;
    }
    return 0;
}

/* set memory */
void* memset(void* ptr, int value, size_t n) {
    char* p = (char*)ptr;
    unsigned char v = (unsigned char)value;
    while (n > 0) {
        *p++ = v;
        n--;
    }
    return ptr;
}

/* character classification */
int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

int isalpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isalnum(int c) {
    return isdigit(c) || isalpha(c);
}

int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

int toupper(int c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

int tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 'a';
    }
    return c;
}

/* string tokenizer */
char* strtok(char* str, const char* delim) {
    static char* token_ptr = NULL;
    
    if (str != NULL) {
        token_ptr = str;
    }
    
    if (token_ptr == NULL) {
        return NULL;
    }
    
    /* skip leading delimiters */
    while (*token_ptr != '\0' && strchr(delim, *token_ptr) != NULL) {
        token_ptr++;
    }
    
    if (*token_ptr == '\0') {
        return NULL;
    }
    
    char* start = token_ptr;
    
    /* find end of token */
    while (*token_ptr != '\0' && strchr(delim, *token_ptr) == NULL) {
        token_ptr++;
    }
    
    if (*token_ptr != '\0') {
        *token_ptr = '\0';
        token_ptr++;
    }
    
    return start;
}

/* find substring */
char* strstr(const char* haystack, const char* needle) {
    if (*needle == '\0') {
        return (char*)haystack;
    }
    
    for (size_t i = 0; haystack[i] != '\0'; i++) {
        size_t j = 0;
        while (haystack[i + j] == needle[j] && needle[j] != '\0') {
            j++;
        }
        if (needle[j] == '\0') {
            return (char*)(haystack + i);
        }
    }
    
    return NULL;
}

/* string to unsigned long */
unsigned long strtoul(const char* str, char** endptr, int base) {
    unsigned long result = 0;
    const char* p = str;
    
    /* skip leading whitespace */
    while (isspace(*p)) {
        p++;
    }
    
    /* handle optional sign */
    int negative = 0;
    if (*p == '-') {
        negative = 1;
        p++;
    } else if (*p == '+') {
        p++;
    }
    
    /* handle base */
    if (base == 0) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base = 16;
            p += 2;
        } else if (p[0] == '0') {
            base = 8;
        } else {
            base = 10;
        }
    }
    
    /* convert digits */
    while (*p != '\0') {
        int digit;
        if (isdigit(*p)) {
            digit = *p - '0';
        } else if (*p >= 'a' && *p <= 'f') {
            digit = *p - 'a' + 10;
        } else if (*p >= 'A' && *p <= 'F') {
            digit = *p - 'A' + 10;
        } else {
            break;
        }
        
        if (digit >= base) {
            break;
        }
        
        result = result * base + digit;
        p++;
    }
    
    if (endptr != NULL) {
        *endptr = (char*)p;
    }
    
    return negative ? -result : result;
}

/* string to long */
long strtol(const char* str, char** endptr, int base) {
    return (long)strtoul(str, endptr, base);
}

/* reverse string */
void reverse(char* str, int len) {
    int start = 0;
    int end = len - 1;
    
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

/* integer to string */
int int_to_str(int num, char* str, int base) {
    if (base < 2 || base > 36) {
        return 0;
    }
    
    int is_negative = 0;
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    int i = 0;
    if (num == 0) {
        str[i++] = '0';
    }
    
    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem < 10) ? '0' + rem : 'A' + rem - 10;
        num = num / base;
    }
    
    if (is_negative) {
        str[i++] = '-';
    }
    
    str[i] = '\0';
    reverse(str, i);
    
    return i;
}

/* simplified sprintf */
int sprintf(char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsprintf(str, format, args);
    va_end(args);
    return result;
}

/* simplified snprintf */
int snprintf(char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}

/* variable argument sprintf */
int vsprintf(char* str, const char* format, va_list args) {
    int i = 0;
    
    for (const char* p = format; *p != '\0'; p++) {
        if (*p == '%' && *(p + 1) != '\0') {
            p++; /* skip % */
            
            switch (*p) {
                case 'd': {
                    int val = va_arg(args, int);
                    char temp[32];
                    int_to_str(val, temp, 10);
                    strcpy(str + i, temp);
                    i += strlen(temp);
                    break;
                }
                case 'u': {
                    unsigned int val = va_arg(args, unsigned int);
                    char temp[32];
                    int_to_str(val, temp, 10);
                    strcpy(str + i, temp);
                    i += strlen(temp);
                    break;
                }
                case 'x': {
                    unsigned int val = va_arg(args, unsigned int);
                    char temp[32];
                    int_to_str(val, temp, 16);
                    strcpy(str + i, temp);
                    i += strlen(temp);
                    break;
                }
                case 'c': {
                    char val = (char)va_arg(args, int);
                    str[i++] = val;
                    break;
                }
                case 's': {
                    char* val = va_arg(args, char*);
                    strcpy(str + i, val);
                    i += strlen(val);
                    break;
                }
                case '%': {
                    str[i++] = '%';
                    break;
                }
                default:
                    str[i++] = *p;
                    break;
            }
        } else {
            str[i++] = *p;
        }
    }
    
    str[i] = '\0';
    return i;
}

/* variable argument snprintf */
int vsnprintf(char* str, size_t size, const char* format, va_list args) {
    int result = vsprintf(str, format, args);
    if (result >= size) {
        str[size - 1] = '\0';
        return size - 1;
    }
    return result;
}

/* simplified memory allocation */
static char memory_pool[1024 * 1024]; /* 1mb pool */
static size_t memory_used = 0;

/* allocate memory */
void* malloc(size_t size) {
    if (memory_used + size > sizeof(memory_pool)) {
        return NULL;
    }
    
    void* ptr = &memory_pool[memory_used];
    memory_used += size;
    
    /* align to 8 bytes */
    memory_used = (memory_used + 7) & ~7;
    
    return ptr;
}

/* free memory (simplified - no actual deallocation) */
void free(void* ptr) {
    /* simplified - we don't actually free memory in this minimal implementation */
    (void)ptr;
}

/* find character in string */
char* strchr(const char* str, int c) {
    while (*str != '\0') {
        if (*str == (char)c) {
            return (char*)str;
        }
        str++;
    }
    return NULL;
}

/* absolute value */
int abs(int x) {
    return x < 0 ? -x : x;
}