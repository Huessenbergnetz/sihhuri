#!/bin/bash

PRJNAME=sihhuri
GPGKEY=6607CA3F41B25F45

if [ $# -lt 2 ]
then
    echo "Usage: ./makerelease.sh <version> <outputdir> [--no-signing]"
    exit 1
fi

VERSION=$1
shift
OUTPUTDIR=$1
shift

SIGNTARBALL=1

TARCMD=`which tar`
if [ -z $TARCMD ]
then
    echo "Can not find GNU tar";
    exit 1;
fi

XZCMD=`which xz`
if [ -z $XZCMD ]
then
    echo "Can not find xz";
    exit 1;
fi

if [ -z $VERSION ]
then
    echo "Missing version";
    exit 1;
fi

if [ -z "$OUTPUTDIR" ]
then
    echo "Missing output directory";
    exit 2;
fi

while [ -n "$1" ]
do
    case $1 in
        --no-signing) SIGNTARBALL=0;;
    esac
    shift
done

TMPDIR=$(mktemp -d)

DIR="${TMPDIR}/${PRJNAME}-${VERSION}"

mkdir $DIR

for SRCDIR in src translations service
do
    cp -r $SRCDIR $DIR
done

for SRCFILE in CMakeLists.txt LICENSE README.md
do
    cp $SRCFILE $DIR
done

rm -f "${OUTPUTDIR}/${PRJNAME}-${VERSION}.tar.xz"

pushd $TMPDIR
$TARCMD -cf "${OUTPUTDIR}/${PRJNAME}-${VERSION}.tar" ${PRJNAME}-${VERSION}
popd

rm -rf $TMPDIR

if [ $SIGNTARBALL -gt 0 ]
then
    GPGCMD=`which gpg`

    if [ -z $GPGCMD ]
    then
        echo "Can not find gpg. Skipping tarball signing..."
    else
        $GPGCMD --armor --detach-sign --yes --default-key $GPGKEY --output "${OUTPUTDIR}/${PRJNAME}-${VERSION}.tar.sig" "${OUTPUTDIR}/${PRJNAME}-${VERSION}.tar"
    fi

fi

$XZCMD "${OUTPUTDIR}/${PRJNAME}-${VERSION}.tar"

exit 0
