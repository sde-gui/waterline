# SDE_WITH(package, help-string, action-to-detect)
AC_DEFUN([SDE_WITH], [
    AC_ARG_WITH([$1],
        [AS_HELP_STRING(
            [--with-$1],
            [$2]
        )],
        [with_$1=$withval],
        [with_$1=auto]
    )

    AC_MSG_CHECKING([--with-$1])
    AC_MSG_RESULT([$with_$1])

    have_$1=no
    AS_IF([test "x$with_$1" != xno], [
        $3
    ])

    AS_IF([test "x$with_$1" = "xyes" -a "x$have_$1" != "xyes"], [
        AC_MSG_ERROR([$1 requested but not found])
    ])
])
