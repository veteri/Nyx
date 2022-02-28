![Logo](https://i.imgur.com/GUjzfG1.png)
# Nyx
Embed any data into png images using steganography\
**Note: This is a pure CLI tool.**

## Basic usage

Suppose you have 2 images: *tree.png* and *wallpaper.png*\
Using this tool you can embed one into the other.\
For example to embed wallpaper.png into tree.png you'd run following command on the built .exe:

```bash
./Nyx.exe -inject -i tree.png -p wallpaper.png
```

Without this or other steganography tool the injected data is invisible.\
The original image wont change its appearance at all.

To extract the injected data back out, use the following command: 

```bash
./Nyx.exe -extract -i tree_payload.png -o wallpaper.png
```

It is also possible to just check for the existance of a payload\
that has been injected using this tool.

```bash
./Nyx.exe -check tree_payload.png
```

## FAQ

**Q: How much data can i inject into my image?**

A: The tool will tell you if the data you want to inject is too large.
   For a general formula:
   
```c++
long maxPayloadLength = (long)((image->width * image->height * 3) / 8);
```

For a 1080p image this would mean you could inject up to 777kb of data.

**Q: Can i inject anything into an image?**

A: Yes, even executables/DLL's.\
However the amount of data directly depends on how many pixels your source image has.
Check my other repository which was the original motivation for this small helper tool.

**Q: Can i use this to add a hidden watermark to my art?**

A: Yes, but when people reupload your art, it is most likely going to be compressed.\
At this point the watermark is lost.
