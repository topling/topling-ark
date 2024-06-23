#/usr/bin/bash

set -e
set -x
export LC_ALL=C
export LANG=C
if [ -z "$WITH_TBB" ]; then
	export WITH_TBB=
else
	export WITH_TBB=$WITH_TBB
fi

echo $@

if [ "$1" = "oss" ]; then
	if [ -z "$2" ]; then
		echo $0 oss REVISION, REVISION is required
		exit 1
	fi
	export REVISION=$2
fi

ProgArgs="$@"
if [ -z "$PKG_WITH_STATIC" ]; then
	PKG_WITH_STATIC=1
else
	PKG_WITH_STATIC=0
fi

cd ../terark
git tag ${REVISION} || true
cd ../topling-rocks
git tag ${REVISION} || true
#cd ../terichdb
#git tag ${REVISION} || true

if [ `uname` == Darwin ]; then
	cpuNum=`sysctl -n machdep.cpu.thread_count`
else
	cpuNum=`nproc`
fi

function MakeToplingRocks() {
	if [[ `uname` != Darwin && $Compiler != g++-4.7 ]]; then
		cd ../topling-rocks
		env CPU_NUM=$cpuNum PKG_WITH_STATIC=$PKG_WITH_STATIC WITH_TBB=$WITH_TBB $@ \
			sh ../terark/deploy.sh ${ProgArgs[@]}
	fi
}

CompilerList=(
#	/opt/g++-4.8
#	/opt/g++-4.9
#	/opt/g++-5.3
#	/opt/g++-5.4
#	/opt/g++-6.1
#	/opt/g++-6.2
#	/opt/g++-6.3
#	/opt/g++-7.1
#	/opt/g++-7.2
#	/opt/g++-8.2
#	/opt/g++-8.3
#	/opt/g++-8.4
	/opt/g++-9.3
)
OLD_PATH=$PATH
OLD_LD_LIBRARY_PATH=$LD_LIBRARY_PATH
#RLS_FLAGS="-O1 -g3 -DNDEBUG"
for Compiler in ${CompilerList[@]}
do
	if test -e $Compiler/bin; then
		export PATH=$Compiler/bin:$OLD_PATH
		export LD_LIBRARY_PATH=$Compiler/lib64:$OLD_LD_LIBRARY_PATH
		cd ../terark && CPU_NUM=$cpuNum CPU=-march=corei7 BMI2=0 \
			sh ../terark/deploy.sh $@
		MakeToplingRocks CPU=-march=corei7 BMI2=0
		if [[ $Compiler != clang++ ]]; then
			cd ../terark && CPU_NUM=$cpuNum BMI2=1 \
				sh ../terark/deploy.sh $@
			MakeToplingRocks BMI2=1
		fi
	else
		echo Not found compiler: $Compiler
	fi
done

