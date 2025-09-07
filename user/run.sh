#!/bin/bash

#echo "Cleaning old build files.."
#make clean

echo "Configuration file .config file"
make localmodconfig

echo "Starting kernel build..."
make -j6 || { echo "Kernel build failed"; exit 1; }

echo "Installing modules..."
sudo make modules_install -j6 || { echo "Module installation failed"; exit 1; }

echo "Installing kernel..."
sudo make install -j6 || { echo "Kernel installation failed"; exit 1; }

echo "Updating GRUB..."
sudo update-grub || { echo "GRUB update failed"; exit 1; }

echo "Kernel build and installation complete. Please reboot to use the new kernel."


# make -j6 && make modules_install -j6 && make install -j6 && update-grub
