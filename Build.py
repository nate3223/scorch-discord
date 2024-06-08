from dotenv import load_dotenv
import os
import subprocess

load_dotenv()
env = os.environ.copy()

if __name__ == "__main__":
    os.chdir(os.path.dirname(os.path.abspath(__file__)))

    if not os.path.exists("build"):
        os.mkdir("build")
    os.chdir("build")

    subprocess.run(["cmake", ".."], env=env)
    os.chdir("..")
    subprocess.run(["cmake", "--build", "build/", "-j4"], env=env)
