for i in tests/*.sh.log ; do
    echo
    echo $i:
    cat $i
done
