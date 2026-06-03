#!/bin/bash
echo "Welcome to the dependency installer for the Echolink!"
echo "Please help us to find Echolink foulder by copying the path to it and pasting it here."

read -p "Enter the path to the Echolink folder(Enter for current directory): " echolink_path

if [ -z "$echolink_path" ]; then
    echolink_path="."
fi

cd "$echolink_path" || { echo "Directory not found. Please check the path and try again." && exit 1; }

if [ ! -f "CMakeLists.txt" ]; then
    echo "CMakeLists.txt not found in the specified directory. Please check the path and try again."
    exit 1
fi

echo "What dependencies do you want to install?"
echo "1) Client side dependencies (C++, cmake, git)"
echo "2) Server side dependencies (C++, cmake, git, pqxx, postgresql)"

read -p "Enter your choice (1 or 2): " choice


case $choice in 

    1) echo "Installing client side dependencies..."
         if command -v apt &> /dev/null; then
            echo "Found Debian/Ubuntu package manager. Installing dependencies..."
            echo "Updating package lists..."
            sudo apt update
            echo "Installing dependencies..."
            sudo apt install build-essential cmake git -y
         elif command -v pacman &> /dev/null; then
            echo "Found Arch Linux package manager. Installing dependencies..."
            echo "Updating package lists..."
            sudo pacman -Syu --noconfirm
            echo "Installing dependencies..."
            sudo pacman -S  cmake git --noconfirm
         else
            echo "Unsupported package manager. Please install dependencies manually."
            exit 1
         fi
         ;;
    2) echo "Installing server side dependencies..."
         if command -v apt &> /dev/null; then
            echo "Found Debian/Ubuntu package manager. Installing dependencies..."
            echo "Updating package lists..."
            sudo apt update
            echo "Installing dependencies..."
            sudo apt install build-essential cmake git libpqxx-dev postgresql -y
         elif command -v pacman &> /dev/null; then
            echo "Found Arch Linux package manager. Installing dependencies..."
            echo "Updating package lists..."
            sudo pacman -Syu --noconfirm
            echo "Installing dependencies..."
            sudo pacman -S  cmake git libpqxx postgresql --noconfirm
            sudo -u postgres initdb -D /var/lib/postgres/data
         else
            echo "Unsupported package manager. Please install dependencies manually."
            exit 1
         fi
            ;;
    *) echo "Invalid choice. Please run the script again and select either 1 or 2."
         ;;
esac

mkdir -p build && cd build ||  { echo "Failed to create build directory. Please check permissions and try again." && exit 1; }

case $choice in 
    1) echo "Setting up the build for the client..."
       cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SERVER=OFF -DBUILD_CLIENT=ON || { echo "Ошибка CMake"; exit 1; }
       ;;
    2) echo "Setting up the build for the server..."
       cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SERVER=ON -DBUILD_CLIENT=OFF || { echo "Ошибка CMake"; exit 1; }
       ;;
esac

make || { echo "Error occurred while compiling"; exit 1; }

echo "Starting to link built files..."

case $choice in 
    1) sudo ln -sf "$(pwd)/echolink_client" /usr/local/bin/echolink_client || { echo "Failed to create symbolic link for the client. Please check permissions and try again."; exit 1; }
       ;;
    2) sudo ln -sf "$(pwd)/echolink_server" /usr/local/bin/echolink_server || { echo "Failed to create symbolic link for the server. Please check permissions and try again."; exit 1; }
       ;;
esac
echo "Symbolic links created successfully!"

echo "Dependencies installed and Echolink built successfully!"

case $choice in 

    2) echo "Initiating a database setup for the server..."
        sudo systemctl enable --now postgresql || { echo "Failed to start PostgreSQL service. Please check the service status and try again."; exit 1; }
        sudo -u postgres psql <<EOF
CREATE USER echolink_user WITH PASSWORD '14341225';
CREATE DATABASE echolink_db;
GRANT ALL PRIVILEGES ON DATABASE echolink_db TO echolink_user;
EOF

sudo -u postgres psql -d echolink_db <<EOF
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password VARCHAR(255) NOT NULL
);

CREATE TABLE messages (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) NOT NULL,
    content TEXT NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE private_messages (
    id SERIAL PRIMARY KEY,
    sender_username VARCHAR(50) NOT NULL,
    receiver_username VARCHAR(50) NOT NULL,
    content TEXT NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
EOF
        echo "Database setup completed successfully!"
        ;;
esac

exit 0
