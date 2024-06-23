
for f in `find -name '*.cpp' -o -name '*.hpp' -o -name '*.cc'`
do
	sed -r 's/%([+-]?[0-9]*)z([du])/%\1I\2/g' $f > /tmp/zdsed.tmp
	touch -r $f /tmp/zdsed.tmp
	mv -f /tmp/zdsed.tmp $f
done

