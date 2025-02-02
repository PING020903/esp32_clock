#ifndef _USER_PROTOCOL_UTILS_H_
#define _USER_PROTOCOL_UTILS_H_
/* Type of Escape algorithms to be used */
#define NGX_ESCAPE_URI (0)
#define NGX_ESCAPE_ARGS (1)
#define NGX_ESCAPE_URI_COMPONENT (2)
#define NGX_ESCAPE_HTML (3)
#define NGX_ESCAPE_REFRESH (4)
#define NGX_ESCAPE_MEMCACHED (5)
#define NGX_ESCAPE_MAIL_AUTH (6)

/* Type of Unescape algorithms to be used */
#define NGX_UNESCAPE_URI (1)
#define NGX_UNESCAPE_REDIRECT (2)

/**
 * @brief Encode an URI
 *
 * @param dest       a destination memory location
 * @param src        the source string
 * @param len        the length of the source string
 * @return uint32_t  the count of escaped characters
 *
 * @note Please allocate the destination buffer keeping in mind that encoding a
 *       special character will take up 3 bytes (for '%' and two hex digits).
 *       In the worst-case scenario, the destination buffer will have to be 3 times
 *       that of the source string.
 */
uint32_t example_uri_encode(char *dest, const char *src, size_t len);

/**
 * @brief Decode an URI
 *
 * @param dest  a destination memory location
 * @param src   the source string
 * @param len   the length of the source string
 *
 * @note Please allocate the destination buffer keeping in mind that a decoded
 *       special character will take up 2 less bytes than its encoded form.
 *       In the worst-case scenario, the destination buffer will have to be
 *       the same size that of the source string.
 */
void example_uri_decode(char *dest, const char *src, size_t len);
#endif