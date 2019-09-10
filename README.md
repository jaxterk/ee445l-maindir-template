# Main Directory Template

This is a stripped down version of the folder I used for my 445l labs. The lab templates should be cloned into here. The main points of interest are the tivaware folder and the Make.local. Make.local must be edited depending on the path that your arm stuff is installed in and its version. The tivaware stuff is for libraries or something, I'm not really sure.

## To change the remote (push destination) of your git repo:

```
cd ee445l-lab00-template # replace '00' with the lab number
git remote set-url origin <url> # replace <url> with the HTTPS or SSH url of your git repo
```