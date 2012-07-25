/********************************************************************************
* ReactPhysics3D physics library, http://code.google.com/p/reactphysics3d/      *
* Copyright (c) 2010-2012 Daniel Chappuis                                       *
*********************************************************************************
*                                                                               *
* This software is provided 'as-is', without any express or implied warranty.   *
* In no event will the authors be held liable for any damages arising from the  *
* use of this software.                                                         *
*                                                                               *
* Permission is granted to anyone to use this software for any purpose,         *
* including commercial applications, and to alter it and redistribute it        *
* freely, subject to the following restrictions:                                *
*                                                                               *
* 1. The origin of this software must not be misrepresented; you must not claim *
*    that you wrote the original software. If you use this software in a        *
*    product, an acknowledgment in the product documentation would be           *
*    appreciated but is not required.                                           *
*                                                                               *
* 2. Altered source versions must be plainly marked as such, and must not be    *
*    misrepresented as being the original software.                             *
*                                                                               *
* 3. This notice may not be removed or altered from any source distribution.    *
*                                                                               *
********************************************************************************/

// Libraries
#include "GJKAlgorithm.h"
#include "Simplex.h"
#include "../../../constraint/Contact.h"
#include "../../../configuration.h"
#include "../../OverlappingPair.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cassert>
#include <iostream> // TODO : DELETE THIS


// We want to use the ReactPhysics3D namespace
using namespace reactphysics3d;

// Constructor
GJKAlgorithm::GJKAlgorithm(MemoryPool<ContactInfo>& memoryPoolContactInfos)
             :NarrowPhaseAlgorithm(memoryPoolContactInfos), algoEPA(memoryPoolContactInfos) {
    
}

// Destructor
GJKAlgorithm::~GJKAlgorithm() {

}

// Return true and compute a contact info if the two bounding volume collide.
// This method implements the Hybrid Technique for computing the penetration depth by
// running the GJK algorithm on original objects (without margin).
// If the objects don't intersect, this method returns false. If they intersect
// only in the margins, the method compute the penetration depth and contact points
// (of enlarged objects). If the original objects (without margin) intersect, we
// call the computePenetrationDepthForEnlargedObjects() method that run the GJK
// algorithm on the enlarged object to obtain a simplex polytope that contains the
// origin, they we give that simplex polytope to the EPA algorithm which will compute
// the correct penetration depth and contact points between the enlarged objects.
bool GJKAlgorithm::testCollision(const Collider* collider1, const Transform& transform1,
                                 const Collider* collider2,  const Transform& transform2,
		                         ContactInfo*& contactInfo) {
    
    Vector3 suppA;             // Support point of object A
    Vector3 suppB;             // Support point of object B
    Vector3 w;                 // Support point of Minkowski difference A-B
    Vector3 pA;                // Closest point of object A
    Vector3 pB;                // Closest point of object B
    decimal vDotw;
    decimal prevDistSquare;

    // Transform a point from local space of body 2 to local space of body 1 (the GJK algorithm is done in local space of body 1)
    Transform body2Tobody1 = transform1.inverse() * transform2;

    // Matrix that transform a direction from local space of body 1 into local space of body 2
    Matrix3x3 rotateToBody2 = transform2.getOrientation().getMatrix().getTranspose() * transform1.getOrientation().getMatrix();

    // Initialize the margin (sum of margins of both objects)
    decimal margin = 2 * OBJECT_MARGIN;
    decimal marginSquare = margin * margin;
    assert(margin > 0.0);

    // Create a simplex set
    Simplex simplex;

    // Get the previous point V (last cached separating axis)
	Vector3 v = currentOverlappingPair->getCachedSeparatingAxis();

    // Initialize the upper bound for the square distance
    decimal distSquare = DECIMAL_LARGEST;
    
    do {
              
        // Compute the support points for original objects (without margins) A and B
        suppA = collider1->getLocalSupportPoint(-v);
        suppB = body2Tobody1 * collider2->getLocalSupportPoint(rotateToBody2 * v);

        // Compute the support point for the Minkowski difference A-B
        w = suppA - suppB;
        
        vDotw = v.dot(w);
        
        // If the enlarge objects (with margins) do not intersect
        if (vDotw > 0.0 && vDotw * vDotw > distSquare * marginSquare) {
                        
            // Cache the current separating axis for frame coherence
            currentOverlappingPair->setCachedSeparatingAxis(v);
            
            // No intersection, we return false
            return false;
        }

        // If the objects intersect only in the margins
        if (simplex.isPointInSimplex(w) || distSquare - vDotw <= distSquare * REL_ERROR_SQUARE) {
            // Compute the closet points of both objects (without the margins)
            simplex.computeClosestPointsOfAandB(pA, pB);

            // Project those two points on the margins to have the closest points of both
            // object with the margins
            decimal dist = sqrt(distSquare);
            assert(dist > 0.0);
            pA = (pA - (OBJECT_MARGIN / dist) * v);
            pB = body2Tobody1.inverse() * (pB + (OBJECT_MARGIN / dist) * v);

            // Compute the contact info
            Vector3 normal = transform1.getOrientation().getMatrix() * (-v.getUnit());
            decimal penetrationDepth = margin - dist;
			
			// Reject the contact if the penetration depth is negative (due too numerical errors)
			if (penetrationDepth <= 0.0) return false;
			
            // Create the contact info object
            contactInfo = new (memoryPoolContactInfos.allocateObject()) ContactInfo(normal, penetrationDepth, pA, pB);

            // There is an intersection, therefore we return true
            return true;
        }

        // Add the new support point to the simplex
        simplex.addPoint(w, suppA, suppB);

        // If the simplex is affinely dependent
        if (simplex.isAffinelyDependent()) {
            // Compute the closet points of both objects (without the margins)
            simplex.computeClosestPointsOfAandB(pA, pB);

            // Project those two points on the margins to have the closest points of both
            // object with the margins
            decimal dist = sqrt(distSquare);
            assert(dist > 0.0);
            pA = (pA - (OBJECT_MARGIN / dist) * v);
            pB = body2Tobody1.inverse() * (pB + (OBJECT_MARGIN / dist) * v);

            // Compute the contact info
            Vector3 normal = transform1.getOrientation().getMatrix() * (-v.getUnit());
            decimal penetrationDepth = margin - dist;
			
			// Reject the contact if the penetration depth is negative (due too numerical errors)
			if (penetrationDepth <= 0.0) return false;
			
            // Create the contact info object
            contactInfo = new (memoryPoolContactInfos.allocateObject()) ContactInfo(normal, penetrationDepth, pA, pB);

            // There is an intersection, therefore we return true
            return true;
        }

        // Compute the point of the simplex closest to the origin
        // If the computation of the closest point fail
        if (!simplex.computeClosestPoint(v)) {
            // Compute the closet points of both objects (without the margins)
            simplex.computeClosestPointsOfAandB(pA, pB);

            // Project those two points on the margins to have the closest points of both
            // object with the margins
            decimal dist = sqrt(distSquare);
            assert(dist > 0.0);
            pA = (pA - (OBJECT_MARGIN / dist) * v);
            pB = body2Tobody1.inverse() * (pB + (OBJECT_MARGIN / dist) * v);

            // Compute the contact info
            Vector3 normal = transform1.getOrientation().getMatrix() * (-v.getUnit());
            decimal penetrationDepth = margin - dist;
			
			// Reject the contact if the penetration depth is negative (due too numerical errors)
			if (penetrationDepth <= 0.0) return false;
			
            // Create the contact info object
            contactInfo = new (memoryPoolContactInfos.allocateObject()) ContactInfo(normal, penetrationDepth, pA, pB);

            // There is an intersection, therefore we return true
            return true;
        }

        // Store and update the squared distance of the closest point
        prevDistSquare = distSquare;
        distSquare = v.lengthSquare();

        // If the distance to the closest point doesn't improve a lot
        if (prevDistSquare - distSquare <= MACHINE_EPSILON * prevDistSquare) {
            simplex.backupClosestPointInSimplex(v);
            
            // Get the new squared distance
            distSquare = v.lengthSquare();

            // Compute the closet points of both objects (without the margins)
            simplex.computeClosestPointsOfAandB(pA, pB);

            // Project those two points on the margins to have the closest points of both
            // object with the margins
            decimal dist = sqrt(distSquare);
            assert(dist > 0.0);
            pA = (pA - (OBJECT_MARGIN / dist) * v);
            pB = body2Tobody1.inverse() * (pB + (OBJECT_MARGIN / dist) * v);

            // Compute the contact info
            Vector3 normal = transform1.getOrientation().getMatrix() * (-v.getUnit());
            decimal penetrationDepth = margin - dist;
			
			// Reject the contact if the penetration depth is negative (due too numerical errors)
			if (penetrationDepth <= 0.0) return false;
			
            // Create the contact info object
            contactInfo = new (memoryPoolContactInfos.allocateObject()) ContactInfo(normal, penetrationDepth, pA, pB);

            // There is an intersection, therefore we return true
            return true;
        }
    } while(!simplex.isFull() && distSquare > MACHINE_EPSILON * simplex.getMaxLengthSquareOfAPoint());

    // The objects (without margins) intersect. Therefore, we run the GJK algorithm again but on the
    // enlarged objects to compute a simplex polytope that contains the origin. Then, we give that simplex
    // polytope to the EPA algorithm to compute the correct penetration depth and contact points between
    // the enlarged objects.
    return computePenetrationDepthForEnlargedObjects(collider1, transform1, collider2, transform2, contactInfo, v);
}

// This method runs the GJK algorithm on the two enlarged objects (with margin)
// to compute a simplex polytope that contains the origin. The two objects are
// assumed to intersect in the original objects (without margin). Therefore such
// a polytope must exist. Then, we give that polytope to the EPA algorithm to
// compute the correct penetration depth and contact points of the enlarged objects.
bool GJKAlgorithm::computePenetrationDepthForEnlargedObjects(const Collider* const collider1, const Transform& transform1,
                                                             const Collider* const collider2, const Transform& transform2,
                                                             ContactInfo*& contactInfo, Vector3& v) {
    Simplex simplex;
    Vector3 suppA;
    Vector3 suppB;
    Vector3 w;
    decimal vDotw;
    decimal distSquare = DECIMAL_LARGEST;
    decimal prevDistSquare;

    // Transform a point from local space of body 2 to local space of body 1 (the GJK algorithm is done in local space of body 1)
    Transform body2ToBody1 = transform1.inverse() * transform2;

    // Matrix that transform a direction from local space of body 1 into local space of body 2
    Matrix3x3 rotateToBody2 = transform2.getOrientation().getMatrix().getTranspose() * transform1.getOrientation().getMatrix();
    
    do {
        // Compute the support points for the enlarged object A and B
        suppA = collider1->getLocalSupportPoint(-v, OBJECT_MARGIN);
        suppB = body2ToBody1 * collider2->getLocalSupportPoint(rotateToBody2 * v, OBJECT_MARGIN);

        // Compute the support point for the Minkowski difference A-B
        w = suppA - suppB;

        vDotw = v.dot(w);

        // If the enlarge objects do not intersect
        if (vDotw > 0.0) {
            // No intersection, we return false
            return false;
        }

        // Add the new support point to the simplex
        simplex.addPoint(w, suppA, suppB);

        if (simplex.isAffinelyDependent()) {
            return false;
        }

        if (!simplex.computeClosestPoint(v)) {
            return false;
        }

        // Store and update the square distance
        prevDistSquare = distSquare;
        distSquare = v.lengthSquare();

        if (prevDistSquare - distSquare <= MACHINE_EPSILON * prevDistSquare) {
            return false;
        }

    } while(!simplex.isFull() && distSquare > MACHINE_EPSILON * simplex.getMaxLengthSquareOfAPoint());

    // Give the simplex computed with GJK algorithm to the EPA algorithm which will compute the correct
    // penetration depth and contact points between the two enlarged objects
    return algoEPA.computePenetrationDepthAndContactPoints(simplex, collider1, transform1, collider2, transform2, v, contactInfo);
}
