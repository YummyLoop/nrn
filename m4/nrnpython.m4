dnl distutils.sysconfig.get_python_version()
dnl distutils.sysconfig.get_python_inc()

AC_DEFUN([AC_NRN_PYCONF],[
	dnl determine configuration if able to  run python
	ac_nrn_pyconf_val=""
	ac_nrn_pyconf_val=`$4 -c "import distutils.sysconfig
print distutils.sysconfig.$2" | tr -d '\r'`
	if test $? != 0 ; then
		AC_MSG_ERROR([could not run python in order to determine a
configuration variable.])
	fi
	if test "$ac_nrn_pyconf_val" = "" -o "$ac_nrn_pyconf_val" = "None" ; then
		[$1]=[$3]
echo "[$2]  '$ac_nrn_pyconf_val' returning '$[$1]'"
	else
		[$1]=${ac_nrn_pyconf_val}
echo "[$2]  '$ac_nrn_pyconf_val'"
	fi
])

AC_DEFUN([AC_NRN_RUNPYTHON], [
	AC_MSG_CHECKING([if python include files and libraries work])
	zzzLD_LIBRARY_PATH=${LD_LIBRARY_PATH}
	LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${PYLIBDIR}"
	export LD_LIBRARY_PATH
	zzzCFLAGS="$CFLAGS"
	zzzLIBS="$LIBS"
	CFLAGS="$CFLAGS -I${PYINCDIR}"
	LIBS="${PYLIBLINK} $LIBS"
	AC_TRY_RUN([
#include <Python.h>
int main() {
	Py_Initialize();
	Py_Finalize();
	return 0;
}
	],[
		AC_MSG_RESULT(yes)
	],[
		AC_MSG_ERROR(could not run a test that used the python library.
Examine config.log to see error details. Something wrong with
	PYLIB=$PYLIB
or
	PYLIBDIR=$PYLIBDIR
or	
	PYLIBLINK=$PYLIBLINK
or
	PYINCDIR=$PYINCDIR
)
	],[
		AC_MSG_ERROR(Cross compiling not allowed)
	])
	CFLAGS="$zzzCFLAGS"
	LIBS="$zzzLIBS"
	LD_LIBRARY_PATH="$zzzLD_LIBRARY_PATH"
	export LD_LIBRARY_PATH
])

AC_DEFUN([AC_NRN_PYTHON],[

	NRNPYTHON_LIBLA=""
	NRNPYTHON_LIBS=""
	NRNPYTHON_DEP=""
	NRNPYTHON_INCLUDES=""
	NRNPYTHON_PYLIBLINK=""
	build_nrnpython=no

	AC_ARG_WITH([nrnpython],
		AC_HELP_STRING([--with-nrnpython=[desired python binary]],
			[Python interpreter can be used (default is NO)
Probably need to set PYLIBDIR to find libpython...
and PYINCDIR to find Python.h
]),
		[ac_nrn_python=$withval], [ac_nrn_python=no]
	)
	nrn_temp_cflags="$CFLAGS"
	AC_ARG_ENABLE([numpy],
		AC_HELP_STRING([--enable-numpy],
			[allow use of numpy (disabled by default) if python
enabled.
]),
		[ac_nrn_numpy=$enableval], [ac_nrn_numpy=no]
	)
	AC_ARG_ENABLE([cygwin],
		AC_HELP_STRING([--disable-cygwin],
			[build as MINGW program. Only for mswin.]),
		[ac_nrn_cygwin=$enableval], [ac_nrn_cygwin=yes]
	)

	if test "$ac_nrn_python" = "yes" ; then
		ac_nrn_python="python"
	fi

	if test "$ac_nrn_python" != "no" ; then
		
		ac_nrn_python=`which ${ac_nrn_python}`

		if test "$ac_nrn_python" = "" ; then

			AC_MSG_ERROR([Either python is not in the path or the specified python does not exist.])

		fi

		echo "Python binary found ($ac_nrn_python)"

		if test "$CYGWIN" = "yes" ; then
			dnl if python does not use cygwin then neither should we
			if test "$ac_nrn_cygwin" = "yes" ; then
cygcheck "$ac_nrn_python" | grep cygwin1.dll > /dev/null
				if test $? != 0 ; then
					ac_nrn_cygwin=no
					with_memacs=no
					with_readline=no
					with_iv=no
AC_MSG_NOTICE([Because this python is not a CYGWIN program, build as a MinGW program as though
 --disable-cygwin --with-readline=no --without-iv --without-memacs was invoked.
That is, build a version suitable mostly as a Python extension.])
				fi
			fi 
		fi
		AC_MSG_CHECKING([nrnpython configuration])
		NRN_DEFINE(USE_PYTHON,1,[define if Python available])
		if test "$PYVER" = "" ; then
			AC_NRN_PYCONF(xxx,get_python_version(),2.4,$ac_nrn_python)
			PYVER=python${xxx}
		fi
		if test "$PYINCDIR" = "" ; then
			AC_NRN_PYCONF(xxx,get_python_inc(1),"",$ac_nrn_python)
			if test "$xxx" = "" ; then
AC_MSG_ERROR([cannot determine python include directory. Need to
explicitly specify PYINCDIR])
			else
				if test "$CYGWIN" = "yes" ; then xxx="`cygpath -u $xxx`" ; fi
				PYINCDIR="${xxx}"
			fi
		fi
		if test "$EXTRAPYLIBS" = "" ; then
			AC_NRN_PYCONF(EXTRAPYLIBS,get_config_var('LIBS'),"",$ac_nrn_python)
		fi
		setup_extra_link_args=extra_link_args
		case "$host_os" in
			darwin*)
				setup_extra_link_args='#extra_link_args'
				;;
		esac
		if test "$PYLIB" = "" ; then
			case "$host_os" in
			darwin*)
				AC_NRN_PYCONF(xxx,get_config_var('LINKFORSHARED'),"",$ac_nrn_python)
				PYLIBLINK="$xxx"
				PYLIB="$PYLIBLINK"
				;;
			*)
				AC_NRN_PYCONF(xxx,get_config_var('LINKFORSHARED'),"",$ac_nrn_python)
				PYLINKFORSHARED="$xxx"				
				if test "$host_os" = "cygwin" ; then
					AC_NRN_PYCONF(xxx,get_config_var('LIBPL'),"",$ac_nrn_python)
				else
					AC_NRN_PYCONF(xxx,get_config_var('LIBDIR'),"",$ac_nrn_python)
				fi
				if test "$xxx" == "" ; then
					xxx=1
					if test "$host_os" = "cygwin" -a "$ac_nrn_cygwin" = "no" ; then
PYLIBDIR="`dirname $ac_nrn_python`/libs"
if test -d "$PYLIBDIR" ; then
	PYLIB="`ls $PYLIBDIR/libpython*.a 2> /dev/null`"
	if  test "$PYLIB" != "" ; then
		PYLIB=`basename "$PYLIB" | sed 's/lib\(.*\)\.a/\1/'`
		PYLIBLINK="-L${PYLIBDIR} -l${PYLIB}"
		PYLIB="${PYLIBLINK}"
		xxx=0
	fi
fi
					fi
					if test "$xxx" = 1 ; then
AC_MSG_ERROR([Could not determine PYLIBDIR, explicitly set PYLIBDIR, PYLIB,
and PYLIBLINK.])
					fi
				else
PYLIBDIR="${xxx}"
PYLIBLINK="-L${PYLIBDIR} -l${PYVER} ${EXTRAPYLIBS}"
PYLIB="${PYLIBLINK} ${PYLINKFORSHARED} -R${PYLIBDIR}"
				fi
			;;
			esac
		fi
		NRNPYTHON_LIBS="-lnrnpython $PYLIB"
		NRNPYTHON_LIBLA="../nrnpython/libnrnpython.la $PYLIB"
		NRNPYTHON_DEP="../nrnpython/libnrnpython.la"
		NRNPYTHON_INCLUDES="-I${PYINCDIR}"
		NRNPYTHON_PYLIBLINK="$PYLIBLINK"

		if test "$ac_nrn_numpy" = "yes" ; then
			AC_MSG_CHECKING([numpy availability])
			CMD="import numpy;print numpy.__path__@<:@0@:>@ + '''/core/include''' "
			PYTHON_NUMPY_INCLUDE=`${ac_nrn_python} -c "${CMD}"`
			if test "$PYTHON_NUMPY_INCLUDE" != ""; then
				HAVE_NUMPY="yes"
				NRNPYTHON_INCLUDES="${NRNPYTHON_INCLUDES} -I${PYTHON_NUMPY_INCLUDE}"
				NRNPYTHON_DEFINES="-DWITH_NUMPY"
			else
				AC_MSG_ERROR([Python cannot import numpy (numpy not installed?).])
			fi
		else
			echo "numpy not enabled. If desired add --enable-numpy to configure."
			HAVE_NUMPY="no"
			NRNPYTHON_DEFINES="-UWITH_NUMPY"
		fi


		NRNPYTHON_EXEC="${ac_nrn_python}"
		build_nrnpython=yes
		if test "$CYGWIN" = "yes" ; then
			if test "$ac_nrn_cygwin" = "no" ; then
				CFLAGS="-mno-cygwin $CFLAGS"
			fi
		fi
		if test "$enable_bluegene" != yes ; then
			AC_NRN_RUNPYTHON
		fi
	fi
	if test "$CYGWIN" = "yes" ; then
		if test "$ac_nrn_cygwin" = "no" ; then
			CFLAGS="$nrn_temp_cflags"
		fi
	fi

	AC_SUBST(NRNPYTHON_LIBLA)
	AC_SUBST(NRNPYTHON_LIBS)
	AC_SUBST(NRNPYTHON_DEP)
	AC_SUBST(NRNPYTHON_INCLUDES)
	AC_SUBST(NRNPYTHON_DEFINES)
	AC_SUBST(NRNPYTHON_EXEC)
	AC_SUBST(NRNPYTHON_PYLIBLINK)
	AC_SUBST(setup_extra_link_args)
]) dnl end of AC_NRN_PYTHON