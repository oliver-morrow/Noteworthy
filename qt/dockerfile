# docker build . -t davlaf/noteworthy-qt:latest
FROM nginx:alpine

# Set a working directory (inside the Docker container)
WORKDIR /usr/share/nginx/html

# Copy Qt WebAssembly files from the host to the container
# Since the Dockerfile is in the 'noteworthy-qt' folder, use relative paths.
COPY ./build/WebAssembly_Qt_6_8_0_single_threaded-Debug/noteworthy-qt.html ./index.html
COPY ./build/WebAssembly_Qt_6_8_0_single_threaded-Debug/noteworthy-qt.js ./
COPY ./build/WebAssembly_Qt_6_8_0_single_threaded-Debug/noteworthy-qt.wasm ./
COPY ./build/WebAssembly_Qt_6_8_0_single_threaded-Debug/qtloader.js ./
COPY ./build/WebAssembly_Qt_6_8_0_single_threaded-Debug/qtlogo.svg ./

# Expose port 80
EXPOSE 80

# Start nginx
CMD ["nginx", "-g", "daemon off;"]