MTARGET := main
UTARGET := ./WWW/upload

all:$(MTARGET) $(UTARGET)
.PHONY:$(MTARGET) $(UTARGET)
$(MTARGET):main.cpp
	g++ -g -std=c++0x $^ -o $@ -lpthread -lboost_system -lboost_filesystem
$(UTARGET):upload.cpp
	g++ -g -std=c++0x $^ -o $@ -lboost_system -lboost_filesystem
