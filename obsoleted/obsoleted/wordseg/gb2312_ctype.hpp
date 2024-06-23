#ifndef __gb2312_ctype_h__
#define __gb2312_ctype_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#ifdef _MSC_VER

extern unsigned short G_ctype_mask_tab[128];
extern unsigned short G_ctype_mask_tab_w[65536];


inline int ws_isalpha(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x0001;
    else
        return 0;
}

inline int ws_isalnum(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x0002;
    else
        return 0;
}

inline int ws_isdigit(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x0004;
    else
        return 0;
}

inline int ws_isxdigit(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x0008;
    else
        return 0;
}

inline int ws_islower(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x0010;
    else
        return 0;
}

inline int ws_isupper(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x0020;
    else
        return 0;
}

inline int ws_isspace(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x0040;
    else
        return 0;
}

inline int ws_ispunct(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x0080;
    else
        return 0;
}

inline int ws_isprint(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x0100;
    else
        return 0;
}

inline int ws_iscntrl(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x0200;
    else
        return 0;
}

inline int ws_isdelim(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x0400;
    else
        return 0;
}

inline int ws_is_asc_alnum(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x0800;
    else
        return 0;
}

inline int ws_is_asc_alpha(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x1000;
    else
        return 0;
}

inline int ws_is_asc_digit(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x2000;
    else
        return 0;
}

inline int ws_is_asc_punct(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x4000;
    else
        return 0;
}

inline int ws_is_asc_lower(int ch)
{
    if ((unsigned int)(ch) < 128u)
        return G_ctype_mask_tab[ch] & 0x8000;
    else
        return 0;
}


inline int ws_iswalpha(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x0001; }
inline int ws_iswalnum(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x0002; }
inline int ws_iswdigit(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x0004; }
inline int ws_iswxdigit(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x0008; }
inline int ws_iswlower(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x0010; }
inline int ws_iswupper(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x0020; }
inline int ws_iswspace(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x0040; }
inline int ws_iswpunct(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x0080; }
inline int ws_iswprint(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x0100; }
inline int ws_iswcntrl(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x0200; }
inline int ws_iswdelim(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x0400; }
inline int ws_isw_asc_alnum(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x0800; }
inline int ws_isw_asc_alpha(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x1000; }
inline int ws_isw_asc_digit(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x2000; }
inline int ws_isw_asc_punct(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x4000; }
inline int ws_isw_asc_lower(wint_t ch) { return G_ctype_mask_tab_w[ch] & 0x8000; }
#else
#define ws_isalpha isalpha
#define ws_isalnum isalnum
#define ws_isdigit isdigit
#define ws_isxdigit isxdigit
#define ws_islower islower
#define ws_isupper isupper
#define ws_isspace isspace
#define ws_ispunct ispunct
#define ws_isprint isprint
#define ws_iscntrl iscntrl
#define ws_isdelim isdelim
#define ws_is_asc_alnum is_asc_alnum
#define ws_is_asc_alpha is_asc_alpha
#define ws_is_asc_digit is_asc_digit
#define ws_is_asc_punct is_asc_punct
#define ws_is_asc_lower is_asc_lower

#define ws_iswalpha iswalpha
#define ws_iswalnum iswalnum
#define ws_iswdigit iswdigit
#define ws_iswxdigit iswxdigit
#define ws_iswlower iswlower
#define ws_iswupper iswupper
#define ws_iswspace iswspace
#define ws_iswpunct iswpunct
#define ws_iswprint iswprint
#define ws_iswcntrl iswcntrl
#define ws_iswdelim iswdelim
#define ws_isw_asc_alnum isw_asc_alnum
#define ws_isw_asc_alpha isw_asc_alpha
#define ws_isw_asc_digit isw_asc_digit
#define ws_isw_asc_punct isw_asc_punct
#define ws_isw_asc_lower isw_asc_lower

inline int ws_isw_asc_alnum(wint_t ch)
{
	return (unsigned int)(ch) < 128 ? isalnum(ch) : 0;
}

inline int isdelim(int c)
{
	return '_' != c && (isspace(c) || ispunct(c));
}

inline int iswdelim(wint_t c)
{
	return '_' != c && (iswspace(c) || iswpunct(c));
}

#endif


#endif // __gb2312_ctype_h__
