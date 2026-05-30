AC_INIT([NUX_EXAMPLE],[0.0])

AC_ARG_WITH([nux],
	[AS_HELP_STRING(
		[--with-nux],
		[specify location of the NUX repository @<:@default=..@:>@])],
	[],[with_nux=..])
nux_dir=${with_nux}

AC_ARG_WITH([libec],
	[AS_HELP_STRING(
		[--with-libec],
		[specify location of libec @<:@default=<nuxdir>/libec@:>@])],
	[],[with_libec=${with_nux}/libec])
libec_dir=${with_libec}

# Change this with m4 in nux directory, and add a new directory if you include extra macros
AC_CONFIG_MACRO_DIRS([../m4])

AC_MK_DIR([${nux_dir}/mk])
AC_NUXROOT_DIR([${nux_dir}])
AC_LIBEC_DIR([${libec_dir}])


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

machine=${ARCH}

AC_ARG_ENABLE([opt],
	AS_HELP_STRING([--disable-opt],[compile without compiler optimizations]))
AS_IF([test "x$enable_opt" != "xno"],
	[optflags='-O7 -g'],
	[optflags='-O0 -g'])

AC_ARG_ENABLE([werror],
	AS_HELP_STRING([--disable-werror],[compile ignoring compiler warnings]))
AS_IF([test "x$enable_werror" != "xno"],
	[wflags='-Werror'],
	[wflags=''])

AC_SUBST(MACHINE, ${machine})
AC_SUBST(CONFIGURE_FLAGS, ["${optflags} ${wflags} -I\$(SRCROOT)/${nux_dir}/include -I\$(BUILDROOT)/${nux_dir}/include"])
AC_SUBST(APXH_SUBDIRS, [${apxh_subdirs}])
AC_SUBST(INSTALLDIR, ${prefix})

AC_MK_CONFIG_FILES([
	Makefile
	kern/Makefile
	user/Makefile
])


AC_OUTPUT
