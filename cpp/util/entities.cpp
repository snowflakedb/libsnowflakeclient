/*
 * This file is essential for decoding the SAML response used in Okta authentication.
 */

/**
 * Copyright 2012, 2016 Christoph Gärtner
 * Distributed under the Boost Software License, Version 1.0
 */

#include "../include/snowflake/entities.hpp"
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include "snowflake/SF_CRTFunctionSafe.h"

#define UNICODE_MAX 0x10FFFFul

namespace Snowflake
{
  namespace Client
  {

    static const char* const NAMED_ENTITIES[][2] = {
    {"AElig;",    "Æ"},
    {"Aacute;",   "Á"},
    {"Acirc;",    "Â"},
    {"Agrave;",   "À"},
    {"Alpha;",    "Α"},
    {"Aring;",    "Å"},
    {"Atilde;",   "Ã"},
    {"Auml;",     "Ä"},
    {"Beta;",     "Β"},
    {"Ccedil;",   "Ç"},
    {"Chi;",      "Χ"},
    {"Dagger;",   "‡"},
    {"Delta;",    "Δ"},
    {"ETH;",      "Ð"},
    {"Eacute;",   "É"},
    {"Ecirc;",    "Ê"},
    {"Egrave;",   "È"},
    {"Epsilon;",  "Ε"},
    {"Eta;",      "Η"},
    {"Euml;",     "Ë"},
    {"Gamma;",    "Γ"},
    {"Iacute;",   "Í"},
    {"Icirc;",    "Î"},
    {"Igrave;",   "Ì"},
    {"Iota;",     "Ι"},
    {"Iuml;",     "Ï"},
    {"Kappa;",    "Κ"},
    {"Lambda;",   "Λ"},
    {"Mu;",       "Μ"},
    {"Ntilde;",   "Ñ"},
    {"Nu;",       "Ν"},
    {"OElig;",    "Œ"},
    {"Oacute;",   "Ó"},
    {"Ocirc;",    "Ô"},
    {"Ograve;",   "Ò"},
    {"Omega;",    "Ω"},
    {"Omicron;",  "Ο"},
    {"Oslash;",   "Ø"},
    {"Otilde;",   "Õ"},
    {"Ouml;",     "Ö"},
    {"Phi;",      "Φ"},
    {"Pi;",       "Π"},
    {"Prime;",    "″"},
    {"Psi;",      "Ψ"},
    {"Rho;",      "Ρ"},
    {"Scaron;",   "Š"},
    {"Sigma;",    "Σ"},
    {"THORN;",    "Þ"},
    {"Tau;",      "Τ"},
    {"Theta;",    "Θ"},
    {"Uacute;",   "Ú"},
    {"Ucirc;",    "Û"},
    {"Ugrave;",   "Ù"},
    {"Upsilon;",  "Υ"},
    {"Uuml;",     "Ü"},
    {"Xi;",       "Ξ"},
    {"Yacute;",   "Ý"},
    {"Yuml;",     "Ÿ"},
    {"Zeta;",     "Ζ"},
    {"aacute;",   "á"},
    {"acirc;",    "â"},
    {"acute;",    "´"},
    {"aelig;",    "æ"},
    {"agrave;",   "à"},
    {"alefsym;",  "ℵ"},
    {"alpha;",    "α"},
    {"amp;",      "&"},
    {"and;",      "∧"},
    {"ang;",      "∠"},
    {"apos;",     "'"},
    {"aring;",    "å"},
    {"asymp;",    "≈"},
    {"atilde;",   "ã"},
    {"auml;",     "ä"},
    {"bdquo;",    "„"},
    {"beta;",     "β"},
    {"brvbar;",   "¦"},
    {"bull;",     "•"},
    {"cap;",      "∩"},
    {"ccedil;",   "ç"},
    {"cedil;",    "¸"},
    {"cent;",     "¢"},
    {"chi;",      "χ"},
    {"circ;",     "ˆ"},
    {"clubs;",    "♣"},
    {"cong;",     "≅"},
    {"copy;",     "©"},
    {"crarr;",    "↵"},
    {"cup;",      "∪"},
    {"curren;",   "¤"},
    {"dArr;",     "⇓"},
    {"dagger;",   "†"},
    {"darr;",     "↓"},
    {"deg;",      "°"},
    {"delta;",    "δ"},
    {"diams;",    "♦"},
    {"divide;",   "÷"},
    {"eacute;",   "é"},
    {"ecirc;",    "ê"},
    {"egrave;",   "è"},
    {"empty;",    "∅"},
    {"emsp;",     "\xE2\x80\x83"},
    {"ensp;",     "\xE2\x80\x82"},
    {"epsilon;",  "ε"},
    {"equiv;",    "≡"},
    {"eta;",      "η"},
    {"eth;",      "ð"},
    {"euml;",     "ë"},
    {"euro;",     "€"},
    {"exist;",    "∃"},
    {"fnof;",     "ƒ"},
    {"forall;",   "∀"},
    {"frac12;",   "½"},
    {"frac14;",   "¼"},
    {"frac34;",   "¾"},
    {"frasl;",    "⁄"},
    {"gamma;",    "γ"},
    {"ge;",       "≥"},
    {"gt;",       ">"},
    {"hArr;",     "⇔"},
    {"harr;",     "↔"},
    {"hearts;",   "♥"},
    {"hellip;",   "…"},
    {"iacute;",   "í"},
    {"icirc;",    "î"},
    {"iexcl;",    "¡"},
    {"igrave;",   "ì"},
    {"image;",    "ℑ"},
    {"infin;",    "∞"},
    {"int;",      "∫"},
    {"iota;",     "ι"},
    {"iquest;",   "¿"},
    {"isin;",     "∈"},
    {"iuml;",     "ï"},
    {"kappa;",    "κ"},
    {"lArr;",     "⇐"},
    {"lambda;",   "λ"},
    {"lang;",     "〈"},
    {"laquo;",    "«"},
    {"larr;",     "←"},
    {"lceil;",    "⌈"},
    {"ldquo;",    "“"},
    {"le;",       "≤"},
    {"lfloor;",   "⌊"},
    {"lowast;",   "∗"},
    {"loz;",      "◊"},
    {"lrm;",      "\xE2\x80\x8E"},
    {"lsaquo;",   "‹"},
    {"lsquo;",    "‘"},
    {"lt;",       "<"},
    {"macr;",     "¯"},
    {"mdash;",    "—"},
    {"micro;",    "µ"},
    {"middot;",   "·"},
    {"minus;",    "−"},
    {"mu;",       "μ"},
    {"nabla;",    "∇"},
    {"nbsp;",     "\xC2\xA0"},
    {"ndash;",    "–"},
    {"ne;",       "≠"},
    {"ni;",       "∋"},
    {"not;",      "¬"},
    {"notin;",    "∉"},
    {"nsub;",     "⊄"},
    {"ntilde;",   "ñ"},
    {"nu;",       "ν"},
    {"oacute;",   "ó"},
    {"ocirc;",    "ô"},
    {"oelig;",    "œ"},
    {"ograve;",   "ò"},
    {"oline;",    "‾"},
    {"omega;",    "ω"},
    {"omicron;",  "ο"},
    {"oplus;",    "⊕"},
    {"or;",       "∨"},
    {"ordf;",     "ª"},
    {"ordm;",     "º"},
    {"oslash;",   "ø"},
    {"otilde;",   "õ"},
    {"otimes;",   "⊗"},
    {"ouml;",     "ö"},
    {"para;",     "¶"},
    {"part;",     "∂"},
    {"permil;",   "‰"},
    {"perp;",     "⊥"},
    {"phi;",      "φ"},
    {"pi;",       "π"},
    {"piv;",      "ϖ"},
    {"plusmn;",   "±"},
    {"pound;",    "£"},
    {"prime;",    "′"},
    {"prod;",     "∏"},
    {"prop;",     "∝"},
    {"psi;",      "ψ"},
    {"quot;",     "\""},
    {"rArr;",     "⇒"},
    {"radic;",    "√"},
    {"rang;",     "〉"},
    {"raquo;",    "»"},
    {"rarr;",     "→"},
    {"rceil;",    "⌉"},
    {"rdquo;",    "”"},
    {"real;",     "ℜ"},
    {"reg;",      "®"},
    {"rfloor;",   "⌋"},
    {"rho;",      "ρ"},
    {"rlm;",      "\xE2\x80\x8F"},
    {"rsaquo;",   "›"},
    {"rsquo;",    "’"},
    {"sbquo;",    "‚"},
    {"scaron;",   "š"},
    {"sdot;",     "⋅"},
    {"sect;",     "§"},
    {"shy;",      "\xC2\xAD"},
    {"sigma;",    "σ"},
    {"sigmaf;",   "ς"},
    {"sim;",      "∼"},
    {"spades;",   "♠"},
    {"sub;",      "⊂"},
    {"sube;",     "⊆"},
    {"sum;",      "∑"},
    {"sup1;",     "¹"},
    {"sup2;",     "²"},
    {"sup3;",     "³"},
    {"sup;",      "⊃"},
    {"supe;",     "⊇"},
    {"szlig;",    "ß"},
    {"tau;",      "τ"},
    {"there4;",   "∴"},
    {"theta;",    "θ"},
    {"thetasym;", "ϑ"},
    {"thinsp;",   "\xE2\x80\x89"},
    {"thorn;",    "þ"},
    {"tilde;",    "˜"},
    {"times;",    "×"},
    {"trade;",    "™"},
    {"uArr;",     "⇑"},
    {"uacute;",   "ú"},
    {"uarr;",     "↑"},
    {"ucirc;",    "û"},
    {"ugrave;",   "ù"},
    {"uml;",      "¨"},
    {"upsih;",    "ϒ"},
    {"upsilon;",  "υ"},
    {"uuml;",     "ü"},
    {"weierp;",   "℘"},
    {"xi;",       "ξ"},
    {"yacute;",   "ý"},
    {"yen;",      "¥"},
    {"yuml;",     "ÿ"},
    {"zeta;",     "ζ"},
    {"zwj;",      "\xE2\x80\x8D"},
    {"zwnj;",     "\xE2\x80\x8C"}
    };

    static int cmp(const void* key, const void* value)
    {
      return strncmp((const char*)key, *(const char* const*)value,
        strlen(*(const char* const*)value));
    }

#if defined(_WIN32) || defined(_WIN64)
    static int cmp_s(void* pvlocale, const void* key, const void* value)
    {
      //UNUSED(pvlocale);
      return cmp(key, value);
    }
#endif

    static const char* get_named_entity(const char* name)
    {
#if defined(_WIN32) || defined(_WIN64)
      const char* const* entity = (const char* const*)bsearch_s(name,
        NAMED_ENTITIES,
        sizeof NAMED_ENTITIES / sizeof * NAMED_ENTITIES,
        sizeof * NAMED_ENTITIES, cmp_s,
        NULL);
#else
      const char* const* entity = (const char* const*)bsearch(name,
        NAMED_ENTITIES,
        sizeof NAMED_ENTITIES / sizeof * NAMED_ENTITIES,
        sizeof * NAMED_ENTITIES, cmp);
#endif

      return entity ? entity[1] : NULL;
    }

    static size_t utf8_encoded_len(unsigned long cp)
    {
      if (cp <= 0x007Ful) return 1;
      if (cp <= 0x07FFul) return 2;
      if (cp <= 0xFFFFul) return 3;
      if (cp <= 0x10FFFFul) return 4;
      return 0;
    }

    static size_t putc_utf8(unsigned long cp, char* buffer)
    {
      unsigned char* bytes = (unsigned char*)buffer;

      if (cp <= 0x007Ful)
      {
        bytes[0] = (unsigned char)cp;
        return 1;
      }

      if (cp <= 0x07FFul)
      {
        bytes[1] = (unsigned char)((2 << 6) | (cp & 0x3F));
        bytes[0] = (unsigned char)((6 << 5) | (cp >> 6));
        return 2;
      }

      if (cp <= 0xFFFFul)
      {
        bytes[2] = (unsigned char)((2 << 6) | (cp & 0x3F));
        bytes[1] = (unsigned char)((2 << 6) | ((cp >> 6) & 0x3F));
        bytes[0] = (unsigned char)((14 << 4) | (cp >> 12));
        return 3;
      }

      if (cp <= 0x10FFFFul)
      {
        bytes[3] = (unsigned char)((2 << 6) | (cp & 0x3F));
        bytes[2] = (unsigned char)((2 << 6) | ((cp >> 6) & 0x3F));
        bytes[1] = (unsigned char)((2 << 6) | ((cp >> 12) & 0x3F));
        bytes[0] = (unsigned char)((30 << 3) | (cp >> 18));
        return 4;
      }

      return 0;
    }

    static bool parse_entity(
      const char* current, char** to, const char** from, char* dest_end)
    {
      const char* end = strchr(current, ';');
      if (!end) return 0;

      if (current[1] == '#')
      {
        char* tail = NULL;
        int errno_save = errno;
        bool hex = current[2] == 'x' || current[2] == 'X';

        errno = 0;
        unsigned long cp = strtoul(
          current + (hex ? 3 : 2), &tail, hex ? 16 : 10);

        bool fail = errno || tail != end || cp > UNICODE_MAX;
        errno = errno_save;
        if (fail) return 0;

        size_t n = utf8_encoded_len(cp);
        if (n == 0 || *to + n > dest_end) return 0;

        *to += putc_utf8(cp, *to);
        *from = end + 1;

        return 1;
      }
      else
      {
        const char* entity = get_named_entity(&current[1]);
        if (!entity) return 0;

        size_t len = strlen(entity);
        if (*to + len > dest_end) return 0;

        sf_memcpy(*to, (size_t)(dest_end - *to), entity, len);

        *to += len;
        *from = end + 1;

        return 1;
      }
    }

    static size_t bounded_copy(char** to, char* dest_end,
      const char* from, size_t n)
    {
      size_t remaining = (size_t)(dest_end - *to);
      size_t copy = n < remaining ? n : remaining;
      memmove(*to, from, copy);
      *to += copy;
      return copy;
    }

    size_t decode_html_entities_utf8(char* dest, size_t dest_size, const char* src)
    {
      if (!dest || dest_size == 0) return 0;
      if (!src) src = dest;

      char* dest_end = dest + dest_size - 1;
      char* to = dest;
      const char* from = src;

      for (const char* current; (current = strchr(from, '&'));)
      {
        size_t span = (size_t)(current - from);
        if (bounded_copy(&to, dest_end, from, span) < span)
        {
          *to = 0;
          return (size_t)(to - dest);
        }

        if (parse_entity(current, &to, &from, dest_end))
          continue;

        from = current;
        if (to >= dest_end)
        {
          *to = 0;
          return (size_t)(to - dest);
        }
        *to++ = *from++;
      }

      bounded_copy(&to, dest_end, from, strlen(from));
      *to = 0;

      return (size_t)(to - dest);
    }
  }
}

