rsync -rvzh \
	--exclude .git \
	--exclude src \
	--exclude .gitignore \
	--exclude LICENSE \
	--exclude '*.sh' \
	. raghavpant@tool.oi-analytics.com:/data/seasia/ 

