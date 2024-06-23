
set -e
set -x

rm -rf idx2word/{Debug,Release}
rm -rf idx2word/x64
cp -r idx2word $1
sed -i "s/idx2word/$1/" $1/*vcxproj*

for f in $1/*vcxproj*; do
	mv $f `echo "$f" | sed "s/idx2word/$1/g"`
done
