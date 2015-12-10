/*
address_parser.h
----------------

International address parser, designed to use OSM training data,
over 40M addresses formatted with the OpenCage address formatting
templates: https://github.com/OpenCageData/address-formatting.

This is a sequence modeling problem similar to e.g. part-of-speech
tagging, named entity recognition, etc. in which we have a sequence
of inputs (words/tokens) and want to predict a sequence of outputs
(labeled part-of-address tags). This is a supervised learning model
and the training data is created in the Python geodata package
included with this repo. Example record:

en  us  123/house_number Fake/road Street/road Brooklyn/city NY/state 12345/postcode

Where the fields are: {language, country, tagged address}.

After training, the address parser can take as input a tokenized
input string e.g. "123 Fake Street Brooklyn NY 12345" and parse
it into:

{
    "house_number": "123",
    "road": "Fake Street",
    "city": "Brooklyn",
    "state": "NY",
    "postcode": "12345"
}

The model used is a greedy averaged perceptron rather than something
like a CRF since there's ample training data from OSM and the accuracy
on this task is already very high with the simpler model.

However, it is still worth investigating CRFs as they are relatively fast
at prediction time for a small number of tags, can often achieve better
performance and are robust to correlated features, which may not be true
with the general error-driven averaged perceptron.

*/
#ifndef ADDRESS_PARSER_H
#define ADDRESS_PARSER_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "averaged_perceptron.h"
#include "averaged_perceptron_tagger.h"
#include "bloom.h"
#include "libpostal_config.h"
#include "collections.h"
#include "normalize.h"
#include "string_utils.h"

#define DEFAULT_ADDRESS_PARSER_PATH LIBPOSTAL_ADDRESS_PARSER_DIR PATH_SEPARATOR "address_parser.dat"

#define NULL_PHRASE_MEMBERSHIP -1

#define ADDRESS_PARSER_NORMALIZE_STRING_OPTIONS NORMALIZE_STRING_DECOMPOSE | NORMALIZE_STRING_LOWERCASE | NORMALIZE_STRING_LATIN_ASCII
#define ADDRESS_PARSER_NORMALIZE_TOKEN_OPTIONS NORMALIZE_TOKEN_DELETE_HYPHENS | NORMALIZE_TOKEN_DELETE_FINAL_PERIOD | NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS | NORMALIZE_TOKEN_REPLACE_DIGITS

#define ADDRESS_SEPARATOR_NONE 0
#define ADDRESS_SEPARATOR_FIELD_INTERNAL 1 << 0
#define ADDRESS_SEPARATOR_FIELD 1 << 1

#define ADDRESS_PARSER_IS_SEPARATOR(token_type) ((token_type) == COMMA || (token_type) == NEWLINE || (token_type) == HYPHEN || (token_type) == DASH || (token_type) == BREAKING_DASH|| (token_type) == SEMICOLON || (token_type) == PUNCT_OPEN || (token_type) == PUNCT_CLOSE || (token_type) == AT_SIGN )
#define ADDRESS_PARSER_IS_IGNORABLE(token_type) ((token.type) == INVALID_CHAR || (token.type) == PERIOD || (token_type) == COLON )

#define SEPARATOR_LABEL "sep"
#define FIELD_SEPARATOR_LABEL "fsep"


#define ADDRESS_COMPONENT_HOUSE 1 << 0
#define ADDRESS_COMPONENT_HOUSE_NUMBER 1 << 1
#define ADDRESS_COMPONENT_ROAD 1 << 4
#define ADDRESS_COMPONENT_SUBURB 1 << 7
#define ADDRESS_COMPONENT_CITY_DISTRICT 1 << 8
#define ADDRESS_COMPONENT_CITY 1 << 9
#define ADDRESS_COMPONENT_STATE_DISTRICT 1 << 10
#define ADDRESS_COMPONENT_STATE 1 << 11
#define ADDRESS_COMPONENT_POSTAL_CODE 1 << 12
#define ADDRESS_COMPONENT_COUNTRY 1 << 13

enum {
    ADDRESS_PARSER_HOUSE,
    ADDRESS_PARSER_HOUSE_NUMBER,
    ADDRESS_PARSER_ROAD,
    ADDRESS_PARSER_SUBURB,
    ADDRESS_PARSER_CITY_DISTRICT,
    ADDRESS_PARSER_CITY,
    ADDRESS_PARSER_STATE_DISTRICT,
    ADDRESS_PARSER_STATE,
    ADDRESS_PARSER_POSTAL_CODE,
    ADDRESS_PARSER_COUNTRY,
    NUM_ADDRESS_PARSER_TYPES
} address_parser_types;

typedef union address_parser_types {
    uint32_t value;
    struct {
        uint32_t components:16;     // Bitset of components
        uint32_t most_common:16;    // Most common component as short integer enum value 
    };
} address_parser_types_t;


typedef struct address_parser_context {
    char *language;
    char *country;
    cstring_array *features;
    char_array *phrase;
    uint32_array *separators;
    cstring_array *normalized;
    phrase_array *address_dictionary_phrases;
    // Index in address_dictionary_phrases or -1
    int64_array *address_phrase_memberships;
    phrase_array *geodb_phrases;
    // Index in gedob_phrases or -1
    int64_array *geodb_phrase_memberships;
    phrase_array *component_phrases;
    // Index in component_phrases or -1
    int64_array *component_phrase_memberships;
    tokenized_string_t *tokenized_str;
} address_parser_context_t;

typedef struct address_parser_response {
    size_t num_components;
    char **components;
    char **labels;
} address_parser_response_t;

// Can add other gazetteers as well
typedef struct address_parser {
    averaged_perceptron_t *model;
    trie_t *vocab;
    trie_t *phrase_types;
} address_parser_t;

// General usage

address_parser_t *address_parser_new(void);
address_parser_t *get_address_parser(void);
bool address_parser_load(char *dir);

void address_parser_response_destroy(address_parser_response_t *self);
address_parser_response_t *address_parser_parse(char *address, char *language, char *country, address_parser_context_t *context);
void address_parser_destroy(address_parser_t *self);

char *address_parser_normalize_string(char *str);
void address_parser_normalize_token(cstring_array *array, char *str, token_t token);

address_parser_context_t *address_parser_context_new(void);
void address_parser_context_destroy(address_parser_context_t *self);

void address_parser_context_fill(address_parser_context_t *context, address_parser_t *parser, tokenized_string_t *tokenized_str, char *language, char *country);

// Feature function
bool address_parser_features(void *self, void *ctx, tokenized_string_t *str, uint32_t i, char *prev, char *prev2);

// I/O methods

bool address_parser_load(char *dir);
bool address_parser_save(address_parser_t *self, char *output_dir);

// Module setup/teardown

bool address_parser_module_setup(char *dir);
void address_parser_module_teardown(void);


#endif
