AC_INIT([NUX],[0.0])

AC_CONFIG_MACRO_DIRS([m4])

AC_MK_DIR([mk])
AC_NUXROOT_DIR([.])
AC_LIBEC_DIR([libec])
AC_CONTRIBLIB_DIR([libfdt],[libfdt],[contrib/dtc/libfdt])

AC_PREFIX_DEFAULT([${PWD}/build/])

AC_ARG_VAR([ARCH], [The architecture to compile for: i386, amd64, or riscv64])
if test x"${ARCH}" == x ; then
   AC_MSG_ERROR([Please select a valid architecture with `./configure ARCH=<arch>`])
fi

AS_CASE([${ARCH}],
	[i386], [tool_prefix=i686-unknown-elf],
	[amd64], [tool_prefix=amd64-unknown-elf],
	[riscv64], [tool_prefix=riscv64-unknown-elf],

	[*], [AC_MSG_ERROR([Supported Architectures: i386, amd64, riscv64])])


AC_ARG_VAR([TOOLCHAIN],[toolchain prefix @<:@default: <ARCH>-elf@:>@])
if test x"${TOOLCHAIN}" != x ; then
   tool_prefix="${TOOLCHAIN}"
fi

AC_PROG_CC

AC_CHECK_TOOL(TARGET_CC, ${tool_prefix}-gcc, :)
if test x"${TARGET_CC}" = 'x:' ; then
  AC_MSG_ERROR([${tool_prefix}-gcc not found])
fi

AC_CHECK_TOOL(TARGET_LD, ${tool_prefix}-ld, :)
if test x"${TARGET_LD}" = 'x:' ; then
  AC_MSG_ERROR([${tool_prefix}-ld not found])
fi

AC_CHECK_TOOL(TARGET_AR, ${tool_prefix}-ar, :)
if test x"${TARGET_AR}" = 'x:' ; then
  AC_MSG_ERROR([${tool_prefix}-ar not found])
fi

AC_CHECK_TOOL(TARGET_OBJCOPY, ${tool_prefix}-objcopy, :)
if test x"${TARGET_OBJCOPY}" = 'x:' ; then
  AC_MSG_ERROR([${tool_prefix}-objcopy not found])
fi
AC_CHECK_TOOL(TARGET_CXX, ${tool_prefix}-c++, :)
if text x"${TARGET_CXX}" = 'x:'; then
  AC_MSG_ERROR([${tool_prefix}-c++ not found])
fi

machine=${ARCH}

AS_CASE([$machine],
	[i386], [hal=hal_x86 plt=plt_acpi],
	[amd64], [hal=hal_x86 plt=plt_acpi],
	[riscv64], [hal=hal_riscv plt=plt_sbi])


AC_ARG_ENABLE([opt],
	AS_HELP_STRING([--disable-opt],[compile without compiler optimizations]))
AS_IF([test "x$enable_opt" != "xno"],
	[optflags='-O7 -g'],
	[optflags='-O0 -g'])

AC_ARG_ENABLE([werror],
	AS_HELP_STRING([--disable-werror],[compile ignoring compiler warnings]))
AS_IF([test "x$enable_relax" != "xno"],
	[wflags='-Werror'],
	[wflags=''])

debug=1
AC_ARG_ENABLE([debug],
	AS_HELP_STRING([--disable-debug],[compile without debug code and messages]))
AS_IF([test "x$enable_debug" != "xno"],
	[debug=1],
	[debug=0])
AC_SUBST(DEBUG, ${debug})

plt_verbose=0
AC_ARG_ENABLE([plt_verbose],
	AS_HELP_STRING([--enable-plt-verbose],[print extra information from the plt module]))
AS_IF([test "x$enable_plt_verbose" != "xyes"],
	[plt_verbose=0],
	[plt_verbose=1])
AC_SUBST(PLT_VERBOSE, ${plt_verbose})

AC_SUBST(MACHINE, ${machine})
AC_SUBST(HALDIR, lib${hal})
AC_SUBST(PLTDIR, lib${plt})
AC_SUBST(INSTALLDIR, ${prefix})

AC_SUBST(CONFIGURE_FLAGS, ["${optflags} ${wflags} -DNUX_MACHINE=${machine} -I\$(SRCROOT)/include -I\$(BUILDROOT)/include"])

AS_CASE([$hal],
	[hal_x86], [halconf+=libhal_x86/hal.mk],
	[hal_riscv], [halconf+=libhal_riscv/hal.mk])

AS_CASE([$plt],
	[plt_acpi], [pltconf+=libplt_acpi/plt.mk],
	[plt_sbi], [pltconf+=libplt_sbi/plt.mk])

AC_CONFIG_FILES([include/config.h libnux/cfg.mk ${halconf} ${pltconf}])
AC_MK_CONFIG_FILES([
	Makefile
	include/Makefile
	include/nux/Makefile
	libec/Makefile
	libec/machine/Makefile
	libec/sys/Makefile
	libec/riscv64/Makefile
	libec/amd64/Makefile
	libec/i386/Makefile
	libfdt/Makefile
	lib${hal}/Makefile
	lib${plt}/Makefile
	libnux/Makefile
	libnux_user/Makefile
	example/Makefile
	example/kern/Makefile
	example/user/Makefile
	tools/Makefile
	tools/libbfd/Makefile
	tools/ar50/Makefile
	tools/objappend/Makefile
])

AC_CONFIG_SUBDIRS([apxh example])

AC_OUTPUT
