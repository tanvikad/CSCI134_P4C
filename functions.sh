#
# These are functions that are commonly used by sanity check and grading scripts
#

# we have not yet created a directory
TESTDIR=""

# extract the tarball into the specified directory
#   param ... submission prefix
#   param ... submission ID
#   param ... directory to extract into
#
# 	If this function fails, it exits
function unTar {
	# check for parameter presence
	if [ -z "$1" -o -z "$2" -o -z "$3" ]; then
		>&2 echo "USAGE: unTar lab-prefix student-ID directory"
		exit -1
	fi

	# check for existance of tarball
	TARBALL="$1-$2.tar.gz"
	if [ ! -s $TARBALL ]; then
		>&2 echo "FATAL: $TARBALL does not exist!"
		exit -1
	fi

	# check for existance of target directory
	TESTDIR="$3"
	if [ ! -d $TESTDIR ]; then
		>&2 echo "FATAL: target directory $TESTDIR does not exist!"
		exit -1
	fi

	# extract the tarball
	CURDIR=`pwd`
	echo ... using test directory $TESTDIR
	cd $TESTDIR

	echo ... extracting $CURDIR/$TARBALL
	tar xvzf $CURDIR/$TARBALL
	if [ $? -ne 0 ]; then
		>&2 echo "ERROR untaring $TARBALL"
		exit -1
	fi
}

#
# snapshot the initial contents of the directory
#   param ... name of working direcotry
#   param ... PID (for uniqueness)
#
function dirSnap {
	# check for parameter presence
	if [ -z "$1" -o -z "$2" ]; then
		>&2 echo "USAGE: dirSnap working-directory PID"
		exit -1
	fi
	if [ -d "$1" ]; then
		ls -a $1 > /tmp/DIRSNAP.$2
	else
		>&2 echo "ERROR: unable to access working directory $1"
	fi
}

# compare a directory against its initial snapshot
#   param ... name of working direcotry
#   param ... PID (for uniqueness)
function dirCheck {
	if [ -z "$1" -o -z "$2" ]; then
		>&2 echo "USAGE: dirCheck working-directory PID"
		exit -1
	fi

	if [ -d "$1" ]; then
		ls -a $1 > /tmp/DIRCHECK.$2
		cmp /tmp/DIRSNAP.$2 /tmp/DIRCHECK.$2 
		if [ $? -ne 0 ]; then
			echo "Incorrect directory contents:"
			diff /tmp/DIRSNAP.$2 /tmp/DIRCHECK.$2
			return 1
		else
			echo "    restored to freshly untar-ed state ... OK"
			rm -rf /tmp/DIRCHECK.$2
			return 0
		fi
	else
		>&2 echo "ERROR: unable to access working directory $1"
	fi
}

# clean up the testing directory (sanity check only)
#   param ... PID (for temp files)
#   param ... # errors
function cleanup {
	echo ... cleaning up temporary files
	cd $CURDIR
	rm -rf /tmp/DIRSNAP.$1

	if [ -n "$TESTDIR" -a -d "$TESTDIR" ]
	then
		echo ... removing test directory $TESTDIR
		rm -rf $TESTDIR /tmp/DIRSNAP.$1
	fi
		
	echo
	if [ $2 -eq 0 ]; then
		echo "SUBMISSION $TARBALL ... passes sanity check"
		exit 0
	else
		echo "SUBMISSION $TARBALL ... FAILS sanity check with $2 errors"
		exit 1
	fi
}

# extract IDs from the README file
#   param file to check
#   param expected ID
#
#   output ... comma separated IDs
#   returns ... 0 OK, else 1
function getIDs {
	if [ -z "$1" -o -z "$2" ]; then
		>&2 echo "USAGE: getID README expected"
		exit -1
	fi
	if [ ! -s "$1" ]; then
		>&2 echo "ERROR: unable to access $1"
		return 1
	fi

	# get what follows the ID:, less whitespace
	result=`grep "ID:" $1 | cut -d: -f2 | tr -d \[:blank:\] | tr -d "\r" | xargs`
	if [ -z "$result" ]; then
		>&2 echo "$1 contains no ID: string"
		return 1
	fi
	
	# see if there are two submitters
	f1=`echo $result | cut -f1 -d,`
	f2=`echo $result | cut -f2 -d,`

	# confirm ID=submitter
	if [ "$f2" != "$f1" ]; then
		echo $f1,$f2
		if [ "$f1" == "$2" -o "$f2" == "$2" ]; then
			>&2 echo "    two submitters: $f1, $f2"
			return 0;
		else
			>&2 echo "ERROR: $1 ID ($f1,$f2) does not include $2"
			return 1
		fi
	else
		echo $f1
		if [ "$f1" == "$2" ]; then
			>&2 echo "    single submitter: $f1"
			return 0
		else
			>&2 echo "ERROR: $1 ID ($f1) != $2"
			return 1
		fi
	fi
}

# extract E-Mail addresses from the README file
#   param file to check
#
#   output ... black separated Emails
#   returns ... 0 OK, else 1
function getEmail {
	if [ -z "$1" ]; then
		>&2 echo "USAGE: getEmail README"
		exit -1
	fi
	if [ ! -s "$1" ]; then
		>&2 echo "ERROR: unable to access $1"
		return 1
	fi

	# get what follows the EMAIL:, less whitespace
	result=`grep "EMAIL:" $1 | cut -d: -f2 | tr -d \[:space:\] | xargs`
	if [ -z "$result" ]; then
		>&2 echo "$1 contains no EMAIL: string"
		return 1
	fi
	
	# see if there are two submitters
	f1=`echo $result | cut -f1 -d,`
	f2=`echo $result | cut -f2 -d,`
	if [ "$f2" != "$f1" ]; then
		echo $f1,$f2
		ats=`echo -e "$f1\n$f2" | grep '@' | wc -l`
		if [ $ats -eq 2 ]; then
			>&2 echo "    two addresses: $f1, $f2 ... OK"
			return 0
		else
			>&2 echo "    two addresses: $f1, $f2 ... INVALID"
			return 1
		fi
	else
		echo $f1
		ats=`echo $f1 | grep '@' | wc -l`
		if [ $ats -eq 1 ]; then
			>&2 echo "    single address: $f1 ... OK"
			return 0
		else
			>&2 echo "    single address: $f1 ... INVALID"
			return 1
		fi
	fi
}

# extract submitter names from the README file
#   param ... file to check
#
#   output ... name string
#   returns ... 0 OK, else 1
function getName {
	if [ -z "$1" ]; then
		>&2 echo "USAGE: getName README"
		exit -1
	fi
	if [ ! -s "$1" ]; then
		>&2 echo "ERROR: unable to access $1"
		return 1
	fi

	# get what follows the NAME:, less whitespace
	result=`grep "NAME:" $1 | cut -d: -f2 | tr -d "\r" | xargs`
	if [ -z "$result" ]; then
		>&2 echo "$1 contains no NAME: string"
		return 1
	fi
	
	>&2 echo "    submitter(s): $result ... OK"
	echo $result
	return 0
}

# check for the existance of Makefile targets
#   param ... name target
#   param ... name of makefile (opt)
#
#   return 0 OK, else 1
function checkTarget {
	if [ -z "$1" ]; then
		>&2 echo "USAGE: checkTarget target [Makefile]"
		exit -1
	fi

	if [ -z "$2" ]; then
		makefile="Makefile"
	else
		makefile="$2"
	fi

	grep "$1:" $makefile > /dev/null
	if [ $? -eq 0 ]; then
		>&2 echo "    $makefile target $1 ... OK"
		return 0
	else
		>&2 echo "    $makefile target $1 ... NOT FOUND"
		return 1
	fi
}

# check for strings in a Makefile
#   param ... desired string
#   param ... name of makefile (opt)
#
#   return 0 OK, else 1
function checkMakefile {
	if [ -z "$1" ]; then
		>&2 echo "USAGE: checkMakefile string [Makefile]"
		exit -1
	fi

	if [ -z "$2" ]; then
		makefile="Makefile"
	else
		makefile="$2"
	fi

	grep "$1" $makefile > /dev/null
	if [ $? -eq 0 ]; then
		>&2 echo "    $makefile includes $1 ... OK"
		return 0
	else
		>&2 echo "    $makefile includes $1 ... NOT FOUND"
		return 1
	fi
}

# check for the existance of deliverable files
#   parm ... file names
#   return ... number missing
function checkFiles {
	let missing=0
	for i in $*
	do
		if [ -s $i ]; then
			>&2 echo "    $i ... OK"
		else
			>&2 echo "    $i ... NOT PRESENT"
			let missing+=1
		fi
	done
	return $missing
}

# check for the existance of files with expected suffixes
#   parm ... suffixes
#   return ... number missing
function checkSuffixes {
	let missing=0
	for s in $*
	do
		names=`echo *.$s`
		if [ "$names" == '*'.$s ]; then
			let missing+=1
			>&2 echo "    .$s ... NOT PRESENT"
		else
			for f in $names
			do
				>&2 echo "    $f ... OK"
			done
		fi
	done
	return $missing
}

# check for the creation of programs
#   parm ... list of file names
#   return ... number missing
function checkPrograms {
	let missing=0
	for i in $*
	do
		if [ -x $i ]; then
			>&2 echo "    $i ... OK"
		else
			>&2 echo "    $i ... NOT PRESENT"
			let missing+=1
		fi
	done
	return $missing
}

# check a return-code
#   param ... $?
#   param ... expected
function testRC {
	if [ -z "$1" -o -z "$2" ]; then
		>&2 echo "USAGE: testRC got expected"
		exit 1
	fi

	if [ $1 -eq $2 ]; then
		>&2 echo "    RC=$2 ... OK"
		return 0
	else
		>&2 echo "    RC=$2 ... FAIL (RC=$1)"
		return 1
	fi
}

# check no output sent to stderr
#   param ... stderr file
function noOutput {
	if [ -z "$1" ]; then
		>&2 echo "USAGE: STDERR file"
		exit 1
	fi

	if [ ! -s "$1" ]; then
		>&2 echo "    error output ...  NONE"
		return 0
	else
		>&2 echo "    error output ... DUMP FOLLOWS:"
		>&2 cat "$1"
		return 1
	fi
}

# create a manual grading sheet
#   param ... name of file
#   param ... column heading line
#
function createSheet {
	if [ -z "$1" -o -z "$2" ]; then
		>&2 echo "USAGE: createSheet filename.csv headings-line"
		exit 1
	fi

	if [ -s "$1" ]; then
		>&2 echo "Manual grades file $1 ... already exists"
		return 0
	else
		# create a file with a line for every submission
		echo $2 > $1
		commas=`echo $2 | tr -c -d ","`
		ls $assgt-*.tar.gz | sed -e "s/$assgt-//" | sed -e "s/.tar.gz/$commas/" >> $1
		>&2 echo "Manual grades file $1 ... created"
		return 0
	fi
}

# get information from suplemental score/penalty files
#   param ... submission ID
#   param ... score file
#   param ... field #
#   param ... default (defaults to 0)
#
#   outputs score for computation, score for display
function getManual {
	if [ -z "$1" -o -z "$2" -o -z "$3" ]; then
		>&2 echo "USAGE: getManual submission score-file field# [default]"
		exit 1
	fi

	# see if we have a default value to return
	if [ -n "$4" ]; then
		score="$4"
	else
		score="0"
	fi

	# if there is no manual score file
	if [ ! -s "$2" ]; then
		echo $score
		return 1
	fi

	his=`grep "^$1," $2`
	# if there is no entry for this submission
	if [ $? -ne 0 ]; then
		echo $score
		return 1
	fi

	# do we have a value for this field
	f=`echo $his | cut -d, -f$3 | tr -d \[:space:\]`
	if [ -n "$f" ]; then
		echo $f
		return 0
	else
		echo $score
		return 1
	fi
}

# does line contain expected number of fields
#   @param	record to check
#   @param	min # fields
#   @param 	max # fields
#   @return	0 = OK, 1 wrong
function numFields {
	record="$1"
	min=$2
	if [ -z "$3" ]; then
		max=$2
	else
		max=$3
	fi

	fields=`echo $record | tr "," " " | wc -w`
	if [ "$fields" -ge $min -a "$fields" -le $max ]; then
		echo "        number of fields: $fields ... OK"
		return 0
	else
		echo "        number of fields: $fields ... INCORRECT (EXPECTED $min-$max)"
		return 1
	fi
}

# does field contain expected value
#   @param	record to check
#   @param	name of this field
#   @param	field number to check
#   @param	expected value
#
#   @return	0 = OK, 1 wrong
function fieldValue {
	name=$2
	field=$3
	expect=$4
	v=`echo $1 | cut -d, -f$field`
	if [ "$v" = "$expect" ]; then
		echo "        $name (field $field): $v ... OK"
		return 0
	else
		echo "        $name (field $field): $v ... INCORRECT, EXPECTED $expect"
		return 1
	fi
}

# does field contain value in expected range
#   @param	record to check
#   @param	name of this field
#   @param	field number to check
#   @param	minimum value
#   @param	maximum value
#
#   @return	0 = OK, 1 wrong
function fieldRange {
	name=$2
	field=$3
	min=$4
	max=$5
	v=`echo $1 | cut -d, -f$field`
	if [ "$v" -ge "$min" -a "$v" -le "$max" ] ; then
		echo "        $name (field $field): $v ... PLAUSIBLE, (EXPECTED $min-$max)"
		return 0
	else
		echo "        $name (field $field): $v ... IMPLAUSIBLE (EXPECTED $min-$max)"
		return 1
	fi
}
# see if a file contains references to a list of files/symbols
#   param ... name of file to check
#   param ... list of things to be referenced
#
#   return ... number of unreferenced items
function checkReferences {
	file=$1
	let unref=0
	for s in $2
	do
		grep $s $file > /dev/null
		if [ $? -ne 0 ]; then
			echo "    $file does not reference $s"
			let unref+=1
		fi
	done
	return $unref
}

#
# figure out how many slip days are to be used
#    param ... submitter
#    param ... number of late days
#    param ... file containing slip day use
#    param ... file containing slip days available
#
#    outputs number of slip days used on this assignment
#
# FIX: this does not deal with slip days from multiple submitters
#
function getSlips {
	if [ -z "$1" -o -z "$2" -o -z "$3" -o -z "$4" ]; then
		>&2 echo "USAGE: getSlips submitter late-days README-file available-file"
		exit 1
	fi

	who=$1
	late=$2
	readmefile=$3
	availfile=$4

	# no late days -> no slip days
	if [ $late -eq 0 ]; then
		echo 0
		return 0
	fi

	# no available slip days -> no slip days
	avail=`getManual $who $availfile 2 0`
	if ! [ "$avail" -eq "$avail" ] 2>/dev/null
	then
		>&2 echo "WARNING: invalid slip days for $who ($avail)"
		echo 0
		return 0
	fi

	if [ $avail -le 0 ]; then
		echo 0
		return 0
	fi

	# figure out how many they asked for
	result=`grep "SLIPDAYS:" $readmefile`
	if [ $? -ne 0 ]
	then
		echo 0
		return 0
	fi

	# pull off the number of days, and make sure it is a number
	result=`echo $result | cut -d: -f2 | cut -d, -f1 | tr -d \[:space:\]`
	if [ -z "$result" ]
	then
		>&2 echo "WARNING: empty slip days used in $who $readmefile ($result)"
		echo 0
		return 0
	fi
	if ! [ "$result" -eq "$result" ] 2>/dev/null
	then
		>&2 echo "WARNING: invalid slip days used in $who $readmefile ($result)"
		echo 0
		return 0
	fi

	if [ "$result" -le 0 ]; then
		echo 0
		return 0
	fi

	# lesser of late days, available, requested
	if [ $result -gt $late ]; then
		let result=late
	fi
	if [ $result -gt $avail ]; then
		let result=avail
	fi

	echo $result
	return 0
}

#
# penalty_init
#   param ... name of assignment
#   return ... per day penalty
#
function penalty_init {
	# check for required parameters
	if [ -z "$1" ]; then
		>& 2 echo "USAGE: penalty_init assgt_name"
		exit 1;
	else
		assgt="$1"
	fi

	# see if we already have a penalty file
	if [ ! -s "$assgt-penalty" ]; then
		# prompt for and read in a per day penalty
		>&2 echo "What is the per-late day penalty? "
		>&2 echo -n "  (e.g. \"10\" for ten points, or \"10%\" for ten percent): "
		read value
		if [ -z "$value" ]; then
			>&2 echo "There is no late-day penalty"
		else
			# sanity check the entered value (number or percentage)
			if [ "$value" -eq "$value" ] 2>/dev/null
			then
				>&2  echo "Late penalty is $value points/day"
				echo $value > $assgt-penalty
				echo $value
			else 
				# see if it has a percent sign in it
				percent=`echo $value | tr -d "%"`
				if [ "$value" == "$percent" ]
				then
				    # not numeric and doesn't contain a percent
				    >&2 echo "Non-numeric per day late penalty: $value"
				else
				    if [ $percent -eq $percent -a $percent -gt 0 -a $percent -le 100 ] 2>/dev/null
				    then
					>&2  echo "Late penalty is $percent percent/day"
					echo $value > $assgt-penalty
					echo $value
				    else
					>&2  echo "Illegal daily late penalty percentage: $percent"
				    fi
				fi
			fi
		fi
	else
		value=`cat $assgt-penalty`
		>&2 echo "Late day penalty is $value/day"
		echo $value
	fi
}

#
# timeout_init
#   param ... name of assignment
#   param ... default timeout (in seconds)
#   return ... timeout (in seconds)
#
function timeout_init {
	# check for required parameters
	if [ -z "$1" -o -z "$2" ]; then
		>& 2 echo "USAGE: timeout_init assgt_name default"
		exit 1;
	else
		assgt="$1"
		dflt="$2"
	fi

	# see if we already have a timeout file
	if [ ! -s "$assgt-timeout" ]; then
		# prompt for and read in a timeout 
		>&2 echo -n "What is the timeout (in seconds)? "
		read value
		if [ -z "$value" ]; then
			>&2 echo "Default timeout: $dflt"
			echo $dflt > $assgt-timeout
			echo $dflt
		else
			# sanity check the entered value (number or percentage)
			if [ "$value" -eq "$value" ] 2>/dev/null
			then
				>&2  echo "Testing timeout is" $value "seconds"
				echo $value > $assgt-timeout
				echo $value
			else
				>&2 echo "Non-numeric per day timeout: $value"
			fi
		fi
	else
		value=`cat $assgt-timeout`
		>&2 echo "Testing with" $value "second timeout"
		echo $value
	fi
}


#
# download (and optionally build) a file if it does not already exist
#   param ... name of file
#   param ... (base) download URL
#   param ... type (e.g. "c")
#   param ... libs (used to build)
#   return ... 0 = success
#
function downLoad {
	# check for required parameters
	if [ -z "$1" -o -z "$2" ]; then
		>& 2 echo "USAGE: downLoad file url [suffix] [libs]"
		exit 1;
	else
		file=$1
		url=$2
	fi

	# if we already have it, we are done
	if [ -x $file ]; then
		return 0
	fi

	# figure out what type of file we are downloading
	if [ -n "$3" ]; then
		type="$3"
		get="$file.$type"
	else
		get="$file"
		type=""
	fi

	if [ -n "$4" ]; then
		libs="$4"
	else
		libs=""
	fi

	>&2 echo "... downloading $get from $url"
	curl -k -L -o $get $url/$get 2> /dev/null
	if [ $? -ne 0 -o ! -s $get ]; then
		>& 2 echo "FATAL: unable to download test file $url/$get"
		exit 1
	fi

	if [ "$type" == "c" ]; then
		>&2 echo "... building $file"
		gcc -o $file $get $libs
		if [ $? -ne 0 ]; then
			>&2 echo "FATAL: build failure on $get"
			exit 1
		else
			ls -l $file
		fi
	fi

	return 0
}
