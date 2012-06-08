# -*- mode: Makefile; -*-

################################################################################
## --SECTION--                                                            PARSER
################################################################################

################################################################################
### @brief BISON
################################################################################

arangod/Ahuacatl/%.c: @srcdir@/arangod/Ahuacatl/%.y
	@top_srcdir@/config/bison-c.sh $(BISON) $@ $<

################################################################################
### @brief BISON++
################################################################################

lib/JsonParserX/%.cpp: @srcdir@/lib/JsonParserX/%.yy
	@top_srcdir@/config/bison-c++.sh $(BISON) $@ $<

################################################################################
### @brief CLEANUP
################################################################################

CLEANUP += $(BISON_FILES) $(BISONXX_FILES)

################################################################################
## --SECTION--                                                       END-OF-FILE
################################################################################

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
