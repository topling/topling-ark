
if [ $# -lt 2 ]; then
	echo usage $0 oldname newname
	exit 1
fi

old=$1
new=$2

set -x
set -e
git mv $old $new
cd $new
git mv $old.vcxproj $new.vcxproj
git mv $old.vcxproj.filters $new.vcxproj.filters
git mv $old.vcxproj.user    $new.vcxproj.user
sed -ib "s/$old/$new/g" $new.vcxproj*
