This is a port of MuPDF / mudraw to toaruos.

You'll need to get a copy of MuPDF and get it to build; this requires native copies of some things.

If you can get MuPDF's libraries installed (don't worry about the apps), clone this into your `osdev` directory and build it. It will install binaries into the hard disk directory, so rebuild your disk image and run `pdfviewer [filename]`.