# export all variables to sub-make processes
.EXPORT_ALL_VARIABLES: #export

ARCH = intel64

ROOTDIR = $(abspath $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))
INCDIR = $(ROOTDIR)/include
SRCDIR = $(ROOTDIR)/src
BLDDIR = build/$(ARCH)
OUTDIR = lib/$(ARCH)
DOCDIR = documentation

CXXFLAGS = $(NULL)
CFLAGS = $(NULL)
DFLAGS = -DLIBXSTREAM_EXPORTED
IFLAGS = -I$(INCDIR)

OFFLOAD ?= 1
STATIC ?= 1
OMP ?= 0
SYM ?= 0
DBG ?= 0
IPO ?= 0
EXP ?= 0

OUTNAME = $(shell basename $(ROOTDIR))
HEADERS = $(shell ls -1 $(INCDIR)/*.h   2> /dev/null | tr "\n" " ") \
          $(shell ls -1 $(SRCDIR)/*.hpp 2> /dev/null | tr "\n" " ") \
          $(shell ls -1 $(SRCDIR)/*.hxx 2> /dev/null | tr "\n" " ") \
          $(shell ls -1 $(SRCDIR)/*.hh  2> /dev/null | tr "\n" " ")
CPPSRCS = $(shell ls -1 $(SRCDIR)/*.cpp 2> /dev/null | tr "\n" " ")
CXXSRCS = $(shell ls -1 $(SRCDIR)/*.cxx 2> /dev/null | tr "\n" " ")
CCXSRCS = $(shell ls -1 $(SRCDIR)/*.cc  2> /dev/null | tr "\n" " ")
CSOURCS = $(shell ls -1 $(SRCDIR)/*.c   2> /dev/null | tr "\n" " ")
FTNSRCS = $(shell ls -1 $(SRCDIR)/*.f   2> /dev/null | tr "\n" " ")
F77SRCS = $(shell ls -1 $(SRCDIR)/*.F   2> /dev/null | tr "\n" " ")
F90SRCS = $(shell ls -1 $(SRCDIR)/*.f90 2> /dev/null | tr "\n" " ")
FTNINCS = $(shell ls -1 $(DEPDIR)/include/*.f   2> /dev/null | tr "\n" " ")
F77INCS = $(shell ls -1 $(DEPDIR)/include/*.F   2> /dev/null | tr "\n" " ")
F90INCS = $(shell ls -1 $(DEPDIR)/include/*.f90 2> /dev/null | tr "\n" " ")
FTNMODS = $(patsubst %,$(BLDDIR)/%,$(notdir $(FTNINCS:.f=-mod.o)))
F77MODS = $(patsubst %,$(BLDDIR)/%,$(notdir $(F77INCS:.F=-mod77.o)))
F90MODS = $(patsubst %,$(BLDDIR)/%,$(notdir $(F90INCS:.f90=-mod90.o)))
MODULES = $(FTNMODS) $(F77MODS) $(F90MODS)
SOURCES = $(CPPSRCS) $(CXXSRCS) $(CCXSRCS) $(CSOURCS) $(FTNSRCS) $(F77SRCS) $(F90SRCS)
CPPOBJS = $(patsubst %,$(BLDDIR)/%,$(notdir $(CPPSRCS:.cpp=-cpp.o)))
CXXOBJS = $(patsubst %,$(BLDDIR)/%,$(notdir $(CXXSRCS:.cxx=-cxx.o)))
CCXOBJS = $(patsubst %,$(BLDDIR)/%,$(notdir $(CCXSRCS:.cc=-cc.o)))
COBJCTS = $(patsubst %,$(BLDDIR)/%,$(notdir $(CSOURCS:.c=-c.o)))
FTNOBJS = $(patsubst %,$(BLDDIR)/%,$(notdir $(FTNSRCS:.f=-f.o)))
F77OBJS = $(patsubst %,$(BLDDIR)/%,$(notdir $(F77SRCS:.F=-f77.o)))
F90OBJS = $(patsubst %,$(BLDDIR)/%,$(notdir $(F90SRCS:.f90=-f90.o)))
OBJECTS = $(CPPOBJS) $(CXXOBJS) $(CCXOBJS) $(COBJCTS) $(FTNOBJS) $(F77OBJS) $(F90OBJS)

ICPC    = $(notdir $(shell which icpc     2> /dev/null))
ICC     = $(notdir $(shell which icc      2> /dev/null))
IFORT   = $(notdir $(shell which ifort    2> /dev/null))
GPP     = $(notdir $(shell which g++      2> /dev/null))
GCC     = $(notdir $(shell which gcc      2> /dev/null))
GFC     = $(notdir $(shell which gfortran 2> /dev/null))

ifneq (,$(ICPC))
	CXX = $(ICPC)
	ifeq (,$(ICC))
		CC = $(CXX)
	endif
	AR = xiar
else
	CXX = $(GPP)
endif
ifneq (,$(ICC))
	CC = $(ICC)
	ifeq (,$(ICPC))
		CXX = $(CC)
	endif
	AR = xiar
else
	CC = $(GCC)
endif
ifneq (,$(IFORT))
	FC = $(IFORT)
	AR = xiar
else
	FC = $(GFC)
endif
ifneq ($(CXX),)
	LD = $(CXX)
endif
ifeq ($(LD),)
	LD = $(CC)
endif
ifeq ($(LD),)
	LD = $(FC)
endif

ifneq (,$(filter icpc icc ifort,$(CXX) $(CC) $(FC)))
	CXXFLAGS += -fPIC -Wall -std=c++0x
	CFLAGS += -fPIC -Wall -std=c99
	FCFLAGS += -fPIC
	LDFLAGS += -fPIC
	ifeq (0,$(DBG))
		CXXFLAGS += -fno-alias -ansi-alias -O2
		CFLAGS += -fno-alias -ansi-alias -O2
		FCMTFLAGS += -threads
		FCFLAGS += -O2
		DFLAGS += -DNDEBUG
		ifneq ($(IPO),0)
			CXXFLAGS += -ipo
			CFLAGS += -ipo
			FCFLAGS += -ipo
		endif
		ifeq ($(AVX),1)
			TARGET = -xAVX
		else ifeq ($(AVX),2)
			TARGET = -xCORE-AVX2
		else ifeq ($(AVX),3)
			TARGET = -xCOMMON-AVX512
		else
			TARGET = -xHost
		endif
	else
		CXXFLAGS += -O0
		CFLAGS += -O0
		FCFLAGS += -O0
		SYM = $(DBG)
	endif
	ifneq (0,$(SYM))
		ifneq (1,$(SYM))
			CXXFLAGS := -g3 -gdwarf-2 -debug inline-debug-info $(CXXFLAGS)
			CFLAGS := -g3 -gdwarf-2 -debug inline-debug-info $(CFLAGS)
			FCFLAGS := -g $(FCFLAGS)
		else
			CXXFLAGS := -g $(CXXFLAGS)
			CFLAGS := -g $(CFLAGS)
			FCFLAGS := -g $(FCFLAGS)
		endif
	endif
	ifeq (0,$(EXP))
		CXXFLAGS += -fno-exceptions
	endif
	ifneq ($(OMP),0)
		CXXFLAGS += -openmp
		CFLAGS += -openmp
		FCFLAGS += -openmp
		LDFLAGS += -openmp
	endif
	ifeq (0,$(OFFLOAD))
		CXXFLAGS += -no-offload
		CFLAGS += -no-offload
		FCFLAGS += -no-offload
	endif
	ifneq ($(STATIC),0)
		SLDFLAGS += -no-intel-extensions -static-intel
	endif
	FCMODDIRFLAG = -module
else # GCC assumed
	VERSION = $(shell $(GCC) --version | grep "gcc (GCC)" | sed "s/gcc (GCC) \([0-9]\+\.[0-9]\+\.[0-9]\+\).*$$/\1/")
	VERSION_MAJOR = $(shell echo "$(VERSION)" | cut -d"." -f1)
	VERSION_MINOR = $(shell echo "$(VERSION)" | cut -d"." -f2)
	VERSION_PATCH = $(shell echo "$(VERSION)" | cut -d"." -f3)
	MIC = 0
	CXXFLAGS += -Wall -std=c++0x
	CFLAGS += -Wall
	ifneq ($(OS),Windows_NT)
		CXXFLAGS += -fPIC
		CFLAGS += -fPIC
		FCFLAGS += -fPIC
		LDFLAGS += -fPIC
	endif
	ifeq (0,$(DBG))
		CXXFLAGS += -O2 -ftree-vectorize -ffast-math -funroll-loops
		CFLAGS += -O2 -ftree-vectorize -ffast-math -funroll-loops
		FCFLAGS += -O2 -ftree-vectorize -ffast-math -funroll-loops
		DFLAGS += -DNDEBUG
		ifneq ($(IPO),0)
			CXXFLAGS += -flto -ffat-lto-objects
			CFLAGS += -flto -ffat-lto-objects
			FCFLAGS += -flto -ffat-lto-objects
			LDFLAGS += -flto
		endif
		ifeq ($(AVX),1)
			TARGET = -mavx
		else ifeq ($(AVX),2)
			TARGET = -mavx2
		else ifeq ($(AVX),3)
			TARGET = -mavx512f
		else
			TARGET = -march=native
		endif
	else
		CXXFLAGS += -O0
		CFLAGS += -O0
		FCFLAGS += -O0
		SYM = $(DBG)
	endif
	ifneq (0,$(SYM))
		ifneq (1,$(SYM))
			CXXFLAGS := -g3 -gdwarf-2 -debug inline-debug-info $(CXXFLAGS)
			CFLAGS := -g3 -gdwarf-2 -debug inline-debug-info $(CFLAGS)
			FCFLAGS := -g $(FCFLAGS)
		else
			CXXFLAGS := -g $(CXXFLAGS)
			CFLAGS := -g $(CFLAGS)
			FCFLAGS := -g $(FCFLAGS)
		endif
	endif
	ifeq (0,$(EXP))
		CXXFLAGS += -fno-exceptions
	endif
	ifneq ($(OMP),0)
		CXXFLAGS += -fopenmp
		CFLAGS += -fopenmp
		FCFLAGS += -fopenmp
		LDFLAGS += -fopenmp
	endif
	ifneq ($(STATIC),0)
		SLDFLAGS += -static
	endif
	FCMODDIRFLAG = -J
endif

ifeq (,$(CXXFLAGS))
	CXXFLAGS = $(CFLAGS)
endif
ifeq (,$(CFLAGS))
	CFLAGS = $(CXXFLAGS)
endif
ifeq (,$(FCFLAGS))
	FCFLAGS = $(CFLAGS)
endif
ifeq (,$(LDFLAGS))
	LDFLAGS = $(CFLAGS)
endif

ifneq ($(STATIC),0)
	LIBEXT = a
else
	LIBEXT = so
endif

parent = $(subst ?, ,$(firstword $(subst /, ,$(subst $(NULL) ,?,$(patsubst ./%,%,$1)))))

.PHONY: all
all: $(OUTDIR)/$(OUTNAME).$(LIBEXT)

$(OUTDIR)/$(OUTNAME).$(LIBEXT): $(OBJECTS)
	@mkdir -p $(dir $@)
ifeq ($(STATIC),0)
	$(LD) -shared -o $@ $(LDFLAGS) $^
else
	$(AR) -rs $@ $^
endif

$(BLDDIR)/%-mod.o: $(DEPDIR)/include/%.f $(ROOTDIR)/Makefile
	@mkdir -p $(dir $@)
	$(FC) $(FCFLAGS) $(FCMTFLAGS) $(DFLAGS) $(IFLAGS) $(TARGET) -c $< -o $@ $(FCMODDIRFLAG) $(dir $@)

$(BLDDIR)/%-mod90.o: $(DEPDIR)/include/%.f90 $(ROOTDIR)/Makefile
	@mkdir -p $(dir $@)
	$(FC) $(FCFLAGS) $(FCMTFLAGS) $(DFLAGS) $(IFLAGS) $(TARGET) -c $< -o $@ $(FCMODDIRFLAG) $(dir $@)

$(BLDDIR)/%-mod77.o: $(DEPDIR)/include/%.F $(ROOTDIR)/Makefile
	@mkdir -p $(dir $@)
	$(FC) $(FCFLAGS) $(FCMTFLAGS) $(DFLAGS) $(IFLAGS) $(TARGET) -c $< -o $@ $(FCMODDIRFLAG) $(dir $@)

$(BLDDIR)/%-cpp.o: $(SRCDIR)/%.cpp $(HEADERS) $(ROOTDIR)/Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DFLAGS) $(IFLAGS) $(TARGET) -c $< -o $@

$(BLDDIR)/%-c.o: $(SRCDIR)/%.c $(HEADERS) $(ROOTDIR)/Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DFLAGS) $(IFLAGS) $(TARGET) -c $< -o $@

$(BLDDIR)/%-f.o: $(SRCDIR)/%.f $(MODULES) $(ROOTDIR)/Makefile
	@mkdir -p $(dir $@)
	$(FC) $(FCFLAGS) $(FCMTFLAGS) $(DFLAGS) $(IFLAGS) $(TARGET) -c $< -o $@

$(BLDDIR)/%-f90.o: $(SRCDIR)/%.f90 $(MODULES) $(ROOTDIR)/Makefile
	@mkdir -p $(dir $@)
	$(FC) $(FCFLAGS) $(FCMTFLAGS) $(DFLAGS) $(IFLAGS) $(TARGET) -c $< -o $@

$(BLDDIR)/%-f77.o: $(SRCDIR)/%.F $(MODULES) $(ROOTDIR)/Makefile
	@mkdir -p $(dir $@)
	$(FC) $(FCFLAGS) $(FCMTFLAGS) $(DFLAGS) $(IFLAGS) $(TARGET) -c $< -o $@

$(DOCDIR)/libxstream.pdf: $(ROOTDIR)/README.md
	@mkdir -p $(dir $@)
	$(eval TEMPLATE := $(shell mktemp --tmpdir=. --suffix=.tex))
	@pandoc -D latex > $(TEMPLATE)
	@sed -i \
		-e 's/\(\\documentclass\[.\+\]{.\+}\)/\1\n\\pagenumbering{gobble}\n\\RedeclareSectionCommands[beforeskip=-1pt,afterskip=1pt]{subsection,subsubsection}/' \
		-e 's/\\usepackage{listings}/\\usepackage{listings}\\lstset{basicstyle=\\footnotesize\\ttfamily}/' \
		$(TEMPLATE)
	@sed \
		-e 's/https:\/\/raw\.githubusercontent\.com\/hfp\/libxstream\/master\///' \
		-e 's/\[\[.\+\](.\+)\]//' \
		-e '/!\[.\+\](.\+)/{n;d}' \
		$(ROOTDIR)/README.md | \
	pandoc \
		--latex-engine=xelatex \
		--template=$(TEMPLATE) --listings \
		-f markdown_github+implicit_figures \
		-V documentclass=scrartcl \
		-V title-meta="LIBXSTREAM Documentation" \
		-V author-meta="Hans Pabst" \
		-V classoption=DIV=45 \
		-V linkcolor=black \
		-V citecolor=black \
		-V urlcolor=black \
		-o $@
	@rm $(TEMPLATE)

$(DOCDIR)/cp2k.pdf: $(ROOTDIR)/documentation/cp2k.md
	@mkdir -p $(dir $@)
	$(eval TEMPLATE := $(shell mktemp --tmpdir=. --suffix=.tex))
	@pandoc -D latex > $(TEMPLATE)
	@sed -i \
		-e 's/\(\\documentclass\[.\+\]{.\+}\)/\1\n\\pagenumbering{gobble}\n\\RedeclareSectionCommands[beforeskip=-1pt,afterskip=1pt]{subsection,subsubsection}/' \
		-e 's/\\usepackage{listings}/\\usepackage{listings}\\lstset{basicstyle=\\footnotesize\\ttfamily}/' \
		$(TEMPLATE)
	@sed \
		-e 's/https:\/\/raw\.githubusercontent\.com\/hfp\/libxstream\/master\///' \
		-e 's/\[\[.\+\](.\+)\]//' \
		-e '/!\[.\+\](.\+)/{n;d}' \
		$(ROOTDIR)/documentation/cp2k.md | \
	pandoc \
		--latex-engine=xelatex \
		--template=$(TEMPLATE) --listings \
		-f markdown_github+implicit_figures \
		-V documentclass=scrartcl \
		-V title-meta="CP2K with LIBXSTREAM" \
		-V author-meta="Hans Pabst" \
		-V classoption=DIV=45 \
		-V linkcolor=black \
		-V citecolor=black \
		-V urlcolor=black \
		-o $@
	@rm $(TEMPLATE)

.PHONY: documentation
documentation: $(DOCDIR)/libxstream.pdf $(DOCDIR)/cp2k.pdf

.PHONY: clean
clean:
ifneq ($(abspath $(call parent,$(BLDDIR))),$(ROOTDIR))
ifneq ($(abspath $(call parent,$(BLDDIR))),$(abspath .))
	@rm -rf $(call parent,$(BLDDIR))
else
	@rm -f $(OBJECTS) $(BLDDIR)/*.mod
endif
else
	@rm -f $(OBJECTS) $(BLDDIR)/*.mod
endif

.PHONY: realclean
realclean: clean
ifneq ($(abspath $(call parent,$(OUTDIR))),$(ROOTDIR))
ifneq ($(abspath $(call parent,$(OUTDIR))),$(abspath .))
	@rm -rf $(call parent,$(OUTDIR))
else
	@rm -f $(OUTDIR)/$(OUTNAME)
endif
else
	@rm -f $(OUTDIR)/$(OUTNAME)
endif

install: all clean
	@cp -r $(INCDIR) . 2> /dev/null || true

