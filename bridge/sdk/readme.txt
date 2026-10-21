Logitech LCD SDK Package
Copyright (C) 2009 Logitech Inc. All Rights Reserved


Important First Step
--------------------------------------------------------------------------

We have modified the structure of the SDK package. It is a must that
you remove any older versions of the SDK, along with all of it's
directories and files, prior to using (unzipping) this new package.


Introduction
--------------------------------------------------------------------------

This package is aimed at anyone that wants to develop an applet for
the Logitech LCD products. The package contains two distinct sets of
SDKs combined:

- Logitech LCD SDK
- Logitech LCD SDK Core

Logitech LCD SDK

This is a set of classes that uses the Core SDK library and that takes
care of implementation details otherwise needed when using the library
directly. It enables developers to quickly and easily display text,
scroll bars and bitmaps.

Logitech LCD SDK Core

This package contains the main LCD library (lglcd.lib) and it's
accompanying header file (lglcd.h). Using this package, a developer
can connect to the LCD Manager and provide his own bitmaps to be
displayed on the LCD.


Environment
--------------------------------------------------------------------------

The environment for use of this package is as follows:
1. Visual C++ 2005 to build and run demo
2. An installed product from Logitech that contains the LCD Manager
   software, such as the G19/G15 Keyboard or the Z-10 Speakers.
3. Logitech LCD SDK package


Disclaimer
--------------------------------------------------------------------------

This is work in progress. If you find anything wrong with either
documentation or code, please let us know so we can improve it.


For questions, problems or suggestions email to:
cj@wingmanteam.com
roland@wingmanteam.com

