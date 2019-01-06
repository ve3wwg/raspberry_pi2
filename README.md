## Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)

by Warren Gay VE3WWG
LGPL2 V2.1

 * http://www.springer.com/us/book/9781484217382


LINUX 4.X Rasbian Jessie (default):

This git branch is for use with Raspbian Linux using a 4.X kernel. This
software has been tested on Raspbian Linux 4.1.14-v7+ for a Raspberry
Pi 2 SoC.

You can check your kernel by doing:

    $ uname -r
    4.1.14-v7+

1. git clone git@github.com:ve3wwg/raspberry_pi2.git

2. (optionally, since LINUX4X should be the default):
   git checkout LINUX4X

3. Follow the make instructions found in the book.


LINUX 3.X Raspbian Wheezy

Perform a git checkout as follows:

1. git clone git@github.com:ve3wwg/raspberry_pi2.git
2. git checkout LINUX3X
3. Follow the make instructions found in the book.

