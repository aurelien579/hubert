all:
	cd hubert && $(MAKE)
	cd restaurant && $(MAKE)
	cd user && $(MAKE)

clean:
	cd hubert && $(MAKE) clean
	cd restaurant && $(MAKE) clean
	cd user && $(MAKE) clean
