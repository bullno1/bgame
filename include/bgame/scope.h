#ifndef BGAME_SCOPE_H
#define BGAME_SCOPE_H

/*! A block run once between two expressions; two on one line collide */
#define BGAME_SCOPE(ENTER, EXIT) \
	for ( \
		int BGAME_UNIQUE_VAR(bgame__scope_) = (ENTER, 0); \
		BGAME_UNIQUE_VAR(bgame__scope_) < 1; \
		++BGAME_UNIQUE_VAR(bgame__scope_), EXIT \
	)

#define BGAME_UNIQUE_VAR(PREFIX) BGAME_CONCAT(PREFIX, __LINE__)

#define BGAME_CONCAT(PREFIX, SUFFIX) BGAME__CONCAT2(PREFIX, SUFFIX)
#define BGAME__CONCAT2(PREFIX, SUFFIX) PREFIX##SUFFIX

#endif
