for a in $1/*.gw
do
  bat -lcpp $a
  ./gwion-lint $a | bat -lcpp
  echo $a; read
done
