#!/bin/sh
#
# Generate dependencies from a list of source files

BUILDC="\\\$\\(LIBTOOL\\) --mode=compile \\\$\\(CC\\) \\\$\\(CFLAGS\\) -c \$src  -o \\\$@"
BUILDCC=$BUILDC
BUILDM=$BUILDC
BUILDASM="\\\$\\(LIBTOOL\\) --tag=CC --mode=compile \\\$\\(auxdir\\)/strip_fPIC.sh \\\$\\(NASM\\) \$src -o \\\$@"

# Check to make sure our environment variables are set
if test x"$INCLUDE" = x -o x"$SOURCES" = x -o x"$objects" = x -o x"$output" = x; then
    echo "SOURCES, INCLUDE, objects, and output needs to be set"
    exit 1
fi
cache_prefix=".#$$"

generate_var()
{
    echo $1 | sed -e 's|^.*/||' -e 's|\.|_|g'
}

search_deps()
{
    base=`echo $1 | sed 's|/[^/]*$||'`
    grep '#include "' <$1 | sed -e 's|.*"\([^"]*\)".*|\1|' | \
    while read file
    do cache=${cache_prefix}_`generate_var $file`
       if test -f $cache; then
          : # We already ahve this cached
       else
           : >$cache
           for path in $base `echo $INCLUDE | sed 's|-I||g'`
           do dep="$path/$file"
              if test -f "$dep"; then
                 echo "	$dep \\" >>$cache
                 search_deps $dep >>$cache
                 break
              fi
           done
       fi
       cat $cache
    done
}

:>${output}.new
for src in $SOURCES
do  echo "Generating dependencies for $src"
    ext=`echo $src | sed 's|.*\.\(.*\)|\1|'`
    obj=`echo $src | sed "s|^.*/\([^ ]*\)\..*|$objects/\1.lo|g"`
    echo "$obj: $src \\" >>${output}.new
    search_deps $src | sort | uniq >>${output}.new
    echo "" >>${output}.new
    case $ext in
        c)   eval echo \\"	$BUILDC\\" >>${output}.new;;
        cc)  eval echo \\"	$BUILDCC\\" >>${output}.new;;
        m)   eval echo \\"	$BUILDM\\" >>${output}.new;;
        asm) eval echo \\"	$BUILDASM\\" >>${output}.new;;
        *)   echo "Unknown file extension: $ext";;
    esac
    echo "" >>${output}.new
done
mv ${output}.new ${output}
