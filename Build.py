import os

if __name__ == "__main__":
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    if not os.path.exists("build"):
        os.mkdir("build")
    os.chdir("build")
    os.system("cmake ..")
    os.chdir("..")
    os.system("cmake --build build/ -j4")
