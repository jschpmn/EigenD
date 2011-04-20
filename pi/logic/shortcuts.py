
#
# Copyright 2009 Eigenlabs Ltd.  http://www.eigenlabs.com
#
# This file is part of EigenD.
#
# EigenD is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# EigenD is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with EigenD.  If not, see <http://www.gnu.org/licenses/>.
#

import terms
import parse

T=terms.make_term
V=terms.make_variable
R=terms.make_rule
S=terms.make_split
E=terms.make_expansion

PC=parse.parse_clause
PCL=parse.parse_clauselist
PT=parse.parse_term
PTL=parse.parse_termlist
PR=parse.parse_rule
PRL=parse.parse_rulelist
