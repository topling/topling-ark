
set -e
set -x

rm -rf dfa2vm/{Debug,Release,x64}
cp -r dfa2vm $1
sed -i "s/dfa2vm/$1/" $1/*vcxproj*

for f in $1/*vcxproj*; do
	mv $f `echo "$f" | sed "s/dfa2vm/$1/g"`
done
