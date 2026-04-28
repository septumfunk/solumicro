#!/bin/sh
dsymutil ../../build/out/desktop/solumicro -o ../../build/out/desktop/solumicro.dSYM
codesign -s - -v -f --entitlements debug-eentitlement.xml ../../build/out/desktop/solumicro