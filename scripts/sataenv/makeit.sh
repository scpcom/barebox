rm -rf env-k1m1 env-k2m2
mkdir env-k1m1 env-k2m2
cp boot_k1m1 env-k1m1
cp boot_k2m2 env-k2m2

../bareboxenv -s ./env-k1m1 k1m1.env
../bareboxenv -s ./env-k2m2 k2m2.env
