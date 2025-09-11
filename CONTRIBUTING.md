# Contributing

## Coding Style

For Linux kernel related code, please follow the [Linux kernel coding style](https://www.kernel.org/doc/html/latest/process/coding-style.html#codingstyle).

You can use the following script to check your code style before submitting a pull request, for example:

```bash
pushd ./sysdrv/source/kernel
./scripts/checkpatch.pl --root sysdrv/source/kernel -f drivers/staging/fbtft/fb_st7789v.c
popd
```
