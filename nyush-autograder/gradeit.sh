#!/bin/bash

echo "****************************************************************"
echo "* CS202 nyush autograder                                       *"
echo "*                                                              *"
echo "* Please note that these sample test cases are not exhaustive. *"
echo "* The test cases for final grading will be different from the  *"
echo "* ones provided and will not be shared. You should test your   *"
echo "* program thoroughly before submission.                        *"
echo "*                                                              *"
echo "* Do not try to hack or exploit the autograder or the CIMS     *"
echo "* computer servers.                                            *"
echo "****************************************************************"

if [ ! -d "inputs" ] || [ ! -d "refoutputs" ] || [ ! -f "print_numbers" ] || [ ! -f "print_numbers_stopped" ]; then
  echo "Missing files. Please re-extract them from the original autograder archive."
  exit 1
fi

echo -e "\e[1;33mExtracting source code...\e[m"
NYUSH_GRADING=nyush-grading
rm -rf $NYUSH_GRADING
mkdir $NYUSH_GRADING
if ! tar xvf nyush-*.tar.xz -C $NYUSH_GRADING; then
  echo -e "\e[1;31mThere was an error extracting your source code. Please copy your nyush-*.tar.xz archive to this directory and try again.\e[m"
  exit 1
fi

echo -e "\e[1;33mCompiling nyush...\e[m"
compile_error() {
  echo -e "\e[1;31mThere was an error compiling nyush. Please make sure your nyush-*.tar.xz archive contains all necessary files and try again.\e[m"
  exit 1
}
module load gcc-9.2
make -C $NYUSH_GRADING || compile_error
[ ! -f $NYUSH_GRADING/nyush ] && compile_error

echo -e "\e[1;33mRunning nyush...\e[m"
killall -q -9 nyush
rm -rf outputs
mkdir -p outputs $NYUSH_GRADING/subdir
cp print_numbers print_numbers_stopped $NYUSH_GRADING
cp print_numbers $NYUSH_GRADING/subdir

preprocess() {
cat << EOF > input.txt
Hello, World
Hello
World
Hello
Lorem Ipsum

This is the end of the input.txt file
EOF

cat << EOF > append.txt
Appending text to
file
in the command
EOF

rm -f output.txt
}

for i in {1..10}; do
  cd $NYUSH_GRADING
	preprocess
	echo "Processing input $i"
	$(timeout --signal=SIGKILL 2 bash -c "cat ../inputs/input$i | ./nyush > ../outputs/output$i 2> ../outputs/erroroutput$i")
  cd ..
done

echo -e "\e[1;33mCleaning up...\e[m"
rm -rf $NYUSH_GRADING

echo -e "\e[1;33mGrading...\e[m"
score=0

for i in {1..10}; do
  echo -ne "\e[1;33mProcessing input $i...\e[m\t"
  if [ ! -f outputs/output$i ] || [ ! -f outputs/erroroutput$i ]; then
    echo -e "\e[1;31mNo output or erroroutput present for this input!\e[m"
    continue
  fi
  diff -b refoutputs/output$i outputs/output$i > /dev/null && result=true || result=false
  diff -b refoutputs/erroroutput$i outputs/erroroutput$i > /dev/null && errorresult=true || errorresult=false
  if $result && $errorresult; then
    echo -e "\e[1;32mCorrect\e[m"
    score=$(($score+1))
  else
    echo -e "\e[1;31mIncorrect\e[m"
    if ! $result; then
      echo -e "\e[1;33mYour output:\e[m"
      cat outputs/output$i
      echo
      echo -e "\e[1;33mExpected output:\e[m"
      cat refoutputs/output$i
      echo
    fi
    if ! $errorresult; then
      echo -e "\e[1;33mYour error output:\e[m"
      cat outputs/erroroutput$i
      echo
      echo -e "\e[1;33mExpected error output:\e[m"
      cat refoutputs/erroroutput$i
      echo
    fi
  fi
done

echo -e "\e[1;33mYou passed $score out of 10 test cases.\e[m"
