# Multisite Creator for Backdrop CMS

This module allows creating new Backdrop multisite websites automatically through a web form.

## Features

- Web form to create new multisite sites
- Automatic creation of necessary directories and files
- Automatic database configuration
- Automatic Backdrop installation
- Automatic Apache Virtual Host configuration
- Automatic SSL certificates with Let's Encrypt
- Email notifications for creation and errors
- Error handling and automatic cleanup on failure
- Weekly activity reports

## Requirements

- This module requires full control of the server where the main site is running
  in.

## Installation

1. Download or clone this module to the `modules/` directory of your main
   Backdrop site
2. Enable the module from Backdrop administration
3. Configure parameters at `Configuration > System > Multisite Creator > Settings`

## Documentation

Find additional information on **requirements** and **installation** in the INSTALL.md
file located in this directory and in the Wiki:
https://github.com/backdrop-contrib/multisite_creator/wiki/Documentation.

## Issues

Bugs and Feature Requests should be reported in the Issue Queue: https://github.com/backdrop-contrib/multisite_creator/issues.

## Current Maintainers

- [Robert Garrigós](https://github.com/robertgarrigos).
- Collaboration and co-maintainers welcome!

## Credits

- Originally written for Backdrop by [Robert Garrigós](https://www.drupal.org/robertgarrigos).

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for
complete text.
