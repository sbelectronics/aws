/* assert.h - a failed assertion prints the expression and where it is, then
 * exits with an error. Defining NDEBUG turns assertions off. */
#undef assert
#ifdef NDEBUG
#define assert(e) ((void)0)
#else
void _assert_fail(const char *expr, const char *file, int line);
#define assert(e) ((e) ? (void)0 : _assert_fail(#e, __FILE__, __LINE__))
#endif
