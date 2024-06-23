
set -e
set -x

rm -rf nlt_build/{Debug,Release,x64}
cp -r nlt_build $1
sed -i "s/nlt_build/$1/" $1/*vcxproj*

for f in $1/*vcxproj*; do
	mv $f `echo "$f" | sed "s/nlt_build/$1/g"`
done
