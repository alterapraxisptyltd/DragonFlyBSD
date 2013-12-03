

.include <bsd.own.mk>

PROG_CXX=clang

SRCDIR=	tools/clang/tools/driver
SRCS=	cc1_main.cpp driver.cpp
NO_MAIN=

LINKS=	${BINDIR}/clang ${BINDIR}/clang++

TGHDRS=	DiagonsticCommonKinds DiagnosticDriverKinds \
	DiagnosticFrontendKinds DiagnosticSemaKinds \
	CC1Options

LIBDEPS=clangfrontend clangdriver clangcodegen \
	clangsema clangchecker clanganalysis \
	clangrewrite clangast clangparse clanglex \
	clangbasic \
	\
	llvminstcombine llvmipo llvmbitwriter \
	llvmbitreader llvmpowerpccodegen \
	llvmpowerpcasmprinter llvmpowerpcinfo \
	llvmx86asmparser llvm ipscodegen \
	llvmipsinfo llvmarmasmparser llvmasmprinter \
	llvmarmcodegen llvmselectiondag llvmasmprinter \
	llvmcodegen llvmscalaropts llvmtransformutils \
	llvmmc llvmcparser llvmmipa llvmanalysis llvmtarget \
	llvmmc llvmcore llvmarminfo llvmsupport llvmsystem

.include "../../clang.prog.mk"
